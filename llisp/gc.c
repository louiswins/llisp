#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "cps.h"
#include "env-private.h"
#include "gc-private.h"
#include "hashtab.h"
#include "obj.h"

/* GC roots */
struct contn *gc_current_contn = NULL;
struct obj *gc_current_obj = NULL;
struct env *gc_global_env = NULL;

static int gc_active = 1;

#define MAX_TEMP_ROOTS 32
static size_t ntemproots = 0;
static void *temp_roots[MAX_TEMP_ROOTS];

static struct obj *all_objects = NULL;
static struct obj *objs_to_mark = NULL;

#ifdef GC_STATS
unsigned long long gc_total_allocs = 0;
unsigned long long gc_total_frees = 0;
#endif

static void *gc_add_to_temp_roots(void *root) {
	if (!gc_active) return root;
	assert(ntemproots < MAX_TEMP_ROOTS);
	return temp_roots[ntemproots++] = root;
}
void gc_cycle() { ntemproots = 0; }

static void gc_mark(struct obj *item);
static void gc_queue(struct obj *obj);

static int is_static(struct obj *obj) {
	return obj == NIL || obj == TRUE || obj == FALSE ||
		obj == (struct obj *) &cbegin || obj == (struct obj *) &cend || obj == (struct obj *) &cfail;
}

static void gc_queue(struct obj *o) {
	if (o == NULL || ISMARKED(o)) return;
	if (is_static(o)) return;
	if (NEXTTOMARK(o) != NULL) return; /* already in queue */
	SETNEXTTOMARK(o, objs_to_mark);
	objs_to_mark = o;
}
static void gc_queue_hashtab_entry(struct string *key, struct obj *value, void *ignored) {
	(void)ignored;
	ADDMARK(key); /* I know it's a string */
	gc_queue(value);
}
static void gc_mark(struct obj *obj) {
	if (ISMARKED(obj)) return;
	ADDMARK(obj);
	switch (TYPE(obj)) {
	default:
		fprintf(stderr, "Fatal error: unknown object type %d\n", TYPE(obj));
		abort();
	case STRING:
	case SYMBOL:
		/* no pointers in a string :) */
		return;
	case HASHTABARR:
		/* Should have been queued as part of its owner, because we don't have the length here */
		/* Could be added as part of the temp roots while allocating. Hopefully if there's
		 * anything in it it's already in the graph, because we don't have size here. */
		/* TODO: change this to a struct {size, array} */
		return;
	case ENV: {
		struct env *env = (struct env *) obj;
		/* If we allocate the environment but then need to collect when trying to allocate
		 * the initial hashtable, we don't have anything to mark yet. */
		if (env->table.cap) {
			ADDMARK(env->table.e);
			hashtab_foreach(&env->table, gc_queue_hashtab_entry, NULL);
		}
		gc_queue((struct obj *) env->parent);
		return;
	}
	case CONTN: {
		struct contn *contn = (struct contn *) obj;
		gc_queue(contn->data);
		gc_queue((struct obj *) contn->env);
		gc_queue((struct obj *) contn->next);
		return;
	}
	case NUM:
	case FN:
	case SPECFORM:
	case BUILTIN:
		return;
	case CELL:
		gc_queue(CAR(obj));
		gc_queue(CDR(obj));
		return;
	case LAMBDA:
	case MACRO: {
		struct closure *cobj = AS_CLOSURE(obj);
		gc_queue(cobj->args);
		gc_queue(cobj->code);
		gc_queue((struct obj *) cobj->env);
		gc_queue((struct obj *) cobj->closurename);
		return;
	}
	}
}

#ifdef DEBUG_GC
static void clear_marks() {
	for (struct obj *cur = all_objects; cur != NULL; cur = cur->next) {
		assert(!ISMARKED(cur));
		DELMARK(cur);
	}
}
#endif

struct gc_reverse_lookup_context {
	struct obj *value;
	struct string *key;
};
static void gc_reverse_hashtab_lookup(struct string *key, struct obj *value, void *context) {
	struct gc_reverse_lookup_context *rlc = context;
	if (value == rlc->value) {
		rlc->key = key;
	}
}
void gc_collect() {
	if (!gc_active || !all_objects) return;
	/* Messing with the interned symbols hashtable can trigger another collection
	 * but collection is not reentrant. Block it. */
	static _Bool collection_active = 0;
	if (collection_active) return;
	collection_active = 1;
#ifdef DEBUG_GC
	clear_marks();
#endif

	/* queue roots */
	gc_queue((struct obj *) gc_current_contn);
	gc_queue(gc_current_obj);
	gc_queue((struct obj *) gc_global_env);
	for (size_t i = 0; i < ntemproots; ++i) {
		gc_queue(temp_roots[i]);
	}
	/* DON'T queue this normally as it's full of weak references */
	if (interned_symbols.cap != 0) {
		ADDMARK(interned_symbols.e);
	}
	/* mark */
	while (objs_to_mark != NULL) {
		struct obj *cur = objs_to_mark;
		objs_to_mark = NEXTTOMARK(cur);
		SETNEXTTOMARK(cur, NULL);
		gc_mark(cur);
	}

	/* sweep */
	struct obj **cur = &all_objects;
	while (*cur != NULL) {
		if (ISMARKED(*cur)) {
			DELMARK(*cur);
			cur = &(*cur)->next;
		} else {
			struct obj *leaked = *cur;
			*cur = leaked->next;
			if (TYPE(leaked) == SYMBOL) {
				/* Clear out weak reference in interned_symbols if necessary
				 * We'll have to figure out something better in case we add weak references somewhere else */
				struct gc_reverse_lookup_context context = { NULL, NULL };
				context.value = leaked;
				hashtab_foreach(&interned_symbols, gc_reverse_hashtab_lookup, &context);
				if (context.key) {
					hashtab_del(&interned_symbols, context.key);
				}
			}
			free(leaked);
#ifdef GC_STATS
			++gc_total_frees;
#endif
		}
	}
	collection_active = 0;
}

struct obj *gc_alloc(enum objtype typ, size_t size) {
#ifdef DEBUG_GC
	gc_collect();
#endif
	struct obj *ret = calloc(1, size);
	if (ret == NULL) {
		gc_collect();
		ret = calloc(1, size);
		if (ret == NULL) {
			fputs("Out of memory\n", stderr);
			abort();
		}
	}
	ret->next = all_objects;
	ret->type = typ;
	all_objects = ret;
#ifdef GC_STATS
	++gc_total_allocs;
#endif
	/* Automatically add the new thing to the temporary roots, until it's firmly
	 * in the object graph */
	return gc_add_to_temp_roots(ret);
}

void gc_suspend() {
	gc_active = 0;
}
void gc_resume() {
	gc_active = 1;
}
