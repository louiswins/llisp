#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "cps.h"
#include "env-private.h"
#include "gc-private.h"
#include "hashtab.h"
#include "obj.h"

#define GCTYPE(o) ((o)->type)

/* GC roots */
struct contn *gc_current_contn = NULL;
struct obj *gc_current_obj = NULL;
struct env *gc_global_env = NULL;

static int gc_active = 1;

#define MAX_TEMP_ROOTS 32
static size_t ntemproots = 0;
static void *temp_roots[MAX_TEMP_ROOTS];

static struct gc_head *all_objects = NULL;
static struct gc_head *objs_to_mark = NULL;

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

static void gc_mark(struct gc_head *gcitem);
static void gc_queue(void *obj);

static int is_static(void *obj) {
	return obj == &nil || obj == &true_ || obj == &false_ ||
		obj == &cbegin || obj == &cend || obj == &cfail;
}

static void gc_queue(void *thing) {
	if (thing == NULL || ISMARKED(GC_FROM_OBJ(thing))) return;
	if (is_static(thing)) return;
	struct gc_head *gc = GC_FROM_OBJ(thing);
	if (NEXTTOMARK(gc) != NULL) return; /* already in queue */
	SETNEXTTOMARK(gc, objs_to_mark);
	objs_to_mark = gc;
}
static void gc_queue_hashtab_entry(struct string *key, struct obj *value, void *ignored) {
	(void)ignored;
	ADDMARK(GC_FROM_OBJ(key)); /* I know it's a string */
	gc_queue(value);
}
/* TODO: delete this after implementing hashtable deletion - no reason to keep
 * the keys alive once we can actually remove them. */
static void gc_queue_hashtab_entry_weak(struct string *key, struct obj *value, void *ignored) {
	(void)value;
	(void)ignored;
	ADDMARK(GC_FROM_OBJ(key));
}
static void gc_mark(struct gc_head *gcitem) {
	if (ISMARKED(gcitem)) return;
	ADDMARK(gcitem);
	if (GCTYPE(gcitem) == GC_STR) return; /* no pointers in a string :) */
	if (GCTYPE(gcitem) == GC_HTENTRY) {
		/* Should be queued as part of its owner, because we don't have the length here */
		/* Could be added as part of the temp roots while allocating. Hopefully if there's
		 * anything in it it's already in the graph, because we don't have size here. */
		/* TODO: change this to a struct {size, array} */
		return;
	}
	if (GCTYPE(gcitem) == GC_ENV) {
		struct env *env = OBJ_FROM_GC(gcitem);
		/* If we allocate the environment but then need to collect when trying to allocate
		 * the initial hashtable, we don't have anything to mark yet. */
		if (env->table.cap) {
			ADDMARK(GC_FROM_OBJ(env->table.e));
			hashtab_foreach(&env->table, gc_queue_hashtab_entry, NULL);
		}
		gc_queue(env->parent);
		return;
	}
	if (GCTYPE(gcitem) == GC_CONTN) {
		struct contn *contn = OBJ_FROM_GC(gcitem);
		gc_queue(contn->data);
		gc_queue(contn->env);
		gc_queue(contn->next);
		return;
	}
	assert(GCTYPE(gcitem) == GC_OBJ);
	struct obj *obj = OBJ_FROM_GC(gcitem);
	switch (TYPE(obj)) {
	default:
		fprintf(stderr, "Fatal error: unknown object type %d\n", TYPE(obj));
		abort();
	case NUM:
	case FN:
	case SPECFORM:
	case BUILTIN:
		return;
	case SYMBOL:
	case STRING:
		gc_queue(obj->str);
		return;
	case CELL:
		gc_queue(obj->head);
		gc_queue(obj->tail);
		return;
	case LAMBDA:
	case MACRO:
		gc_queue(obj->args);
		gc_queue(obj->code);
		gc_queue(obj->env);
		gc_queue(obj->closurename);
		return;
	case CONTN:
		gc_queue(obj->contnp);
		return;
	}
}

#ifdef DEBUG_GC
static void clear_marks() {
	for (struct gc_head *cur = all_objects; cur != NULL; cur = cur->next) {
		assert(!ISMARKED(cur));
		DELMARK(cur);
	}
}
#endif

void gc_collect() {
	if (!gc_active || !all_objects) return;
#ifdef DEBUG_GC
	clear_marks();
#endif

	/* queue roots */
	gc_queue(gc_current_contn);
	gc_queue(gc_current_obj);
	gc_queue(gc_global_env);
	for (size_t i = 0; i < ntemproots; ++i) {
		gc_queue(temp_roots[i]);
	}
	/* DON'T queue this normally as it's full of weak references */
	if (interned_symbols.cap != 0) {
		ADDMARK(GC_FROM_OBJ(interned_symbols.e));
		/* TODO: remove the following line after implementing hashtable deletion */
		hashtab_foreach(&interned_symbols, gc_queue_hashtab_entry_weak, NULL);
	}
	/* mark */
	while (objs_to_mark != NULL) {
		struct gc_head *cur = objs_to_mark;
		objs_to_mark = NEXTTOMARK(cur);
		SETNEXTTOMARK(cur, NULL);
		gc_mark(cur);
	}

	/* sweep */
	struct gc_head **cur = &all_objects;
	while (*cur != NULL) {
		if (ISMARKED(*cur)) {
			DELMARK(*cur);
			cur = &(*cur)->next;
		} else {
			struct gc_head *leaked = *cur;
			*cur = leaked->next;
			if (GCTYPE(leaked) == GC_OBJ) {
				/* Clear out weak reference in interned_symbols if necessary
				 * We'll have to figure out something better in case we add weak references somewhere else */
				struct obj *leaked_obj = (struct obj *)OBJ_FROM_GC(leaked);
				if (TYPE(leaked_obj) == SYMBOL && hashtab_get(&interned_symbols, leaked_obj->str) == leaked_obj) {
					/* TODO: implement hashtable deletion */
					hashtab_put(&interned_symbols, leaked_obj->str, &nil);
				}
			}
			free(leaked);
#ifdef GC_STATS
			++gc_total_frees;
#endif
		}
	}
}

void *gc_alloc(enum gctype typ, size_t size) {
#ifdef DEBUG_GC
	static unsigned char num_allocs = 0;
	if (!++num_allocs) { gc_collect(); }
#endif
	struct gc_head *ret = calloc(1, size);
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
	typedef char assert_intptr_NULL_is_0[(uintptr_t)NULL == 0u ? 1 : -1];
	all_objects = ret;
#ifdef GC_STATS
	++gc_total_allocs;
#endif
	/* Automatically add the new thing to the temporary roots, until it's firmly
	 * in the object graph */
	return gc_add_to_temp_roots(OBJ_FROM_GC(ret));
}

void gc_suspend() {
	gc_active = 0;
}
void gc_resume() {
	gc_active = 1;
}
