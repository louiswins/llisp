#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cps.h"
#include "env-private.h"
#include "gc-private.h"
#include "hashtab.h"
#include "obj.h"
#ifdef GC_STATS
#include "perf.h"
#endif

static uintptr_t *all_allocations = NULL;
static uintptr_t *all_allocations_end = NULL;
static size_t all_allocations_capacity = 0;

static uintptr_t* all_allocations_find_slot(uintptr_t ptr) {
	if (all_allocations == all_allocations_end) return all_allocations;
	if (ptr < all_allocations[0]) return all_allocations;
	if (ptr > *(all_allocations_end - 1)) return all_allocations_end;
	uintptr_t *min = all_allocations;
	uintptr_t *max = all_allocations_end - 1;
	while (min < max) {
		uintptr_t *mid = min + (max - min) / 2;
		if (ptr == *mid) {
			return mid;
		} else if (ptr < *mid) {
			max = mid;
		} else {
			min = mid + 1;
		}
	}
	return min;
}

static void add_allocation(uintptr_t ptr) {
	if (all_allocations_capacity == 0) {
		all_allocations = all_allocations_end = malloc(4 * sizeof(uintptr_t));
		all_allocations_capacity = 4;
	} else if (all_allocations_end == all_allocations + all_allocations_capacity) {
		size_t new_cap = all_allocations_capacity + all_allocations_capacity / 2;
		size_t num_objects = all_allocations_end - all_allocations;
		void *tmp = realloc(all_allocations, new_cap * sizeof(uintptr_t));
		if (!tmp) abort();
		all_allocations = tmp;
		all_allocations_end = all_allocations + num_objects;
		all_allocations_capacity = new_cap;
	}

	uintptr_t *slot = all_allocations_find_slot(ptr);
	if (slot == NULL) abort();
	if (slot != all_allocations_end) {
		if (*slot == ptr) {
			// already there
			return;
		}
		memmove(slot + 1, slot, (all_allocations_end - slot) * sizeof(uintptr_t));
	}
	*slot = ptr;
	++all_allocations_end;
}

static _Bool is_valid_allocation(uintptr_t ptr) {
	assert(all_allocations);
	uintptr_t *slot = all_allocations_find_slot(ptr);
	return slot != all_allocations_end && *slot == ptr;
}

static uintptr_t gc_start_of_stack = 0;
void gc_init(void *bottom_of_stack) {
	uintptr_t bottom = (uintptr_t)bottom_of_stack;
	// round down to correct alignment
	gc_start_of_stack = bottom - bottom % _Alignof(struct obj);
}

static struct obj *objs_to_mark = NULL;

#ifdef GC_STATS
unsigned long long gc_total_allocs = 0;
unsigned long long gc_total_frees = 0;
double time_rootfinding = 0.;
double time_marking = 0.;
double time_sweeping = 0.;
#endif

static void gc_mark(struct obj *item);
static void gc_queue(struct obj *obj);

static void gc_queue(struct obj *o) {
	if ((uintptr_t)o < *all_allocations || (uintptr_t)o > *(all_allocations_end - 1)) return; /* null or static */
	if (ISMARKED(o)) return;
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

_declspec(noinline)
static uintptr_t get_end_of_stack() {
	uintptr_t end_of_stack = (uintptr_t)&end_of_stack;
	// round up to next valid alignment
	end_of_stack += _Alignof(struct obj) - 1;
	end_of_stack -= end_of_stack % _Alignof(struct obj);
	return end_of_stack;
}

void gc_collect() {
	if (all_allocations == all_allocations_end) return;
	/* Messing with the interned symbols hashtable can trigger another collection
	 * but collection is not reentrant. Block it. */
	static _Bool collection_active = 0;
	if (collection_active) return;
	collection_active = 1;

	/* Find roots */
#ifdef GC_STATS
	double start = gettime_perf();
#endif
	jmp_buf jb;
	setjmp(jb);

	uintptr_t end_of_stack = get_end_of_stack();

	if (gc_start_of_stack == 0) abort();
	if (end_of_stack >= gc_start_of_stack) abort();

	for (uintptr_t candidate = gc_start_of_stack; candidate > end_of_stack; candidate -= _Alignof(struct obj)) {
		uintptr_t value_on_stack = *(uintptr_t *)candidate;
		if (!is_valid_allocation(value_on_stack)) {
			continue;
		}
		gc_queue((struct obj *)value_on_stack);
	}

	/* DON'T queue this normally as it's full of weak references */
	if (interned_symbols.cap != 0) {
		ADDMARK(interned_symbols.e);
	}

#ifdef GC_STATS
	double end = gettime_perf();
	time_rootfinding += end - start;
	start = end;
#endif

	/* mark */
	while (objs_to_mark != NULL) {
		struct obj *cur = objs_to_mark;
		objs_to_mark = NEXTTOMARK(cur);
		SETNEXTTOMARK(cur, NULL);
		gc_mark(cur);
	}

#ifdef GC_STATS
	end = gettime_perf();
	time_marking += end - start;
	start = end;
#endif

	/* sweep */
	uintptr_t *writeptr = all_allocations;
	for (uintptr_t *curptr = all_allocations; curptr != all_allocations_end; ++curptr) {
		struct obj *cur = (struct obj *)*curptr;
		if (ISMARKED(cur)) {
			DELMARK(cur);
			*writeptr = *curptr;
			++writeptr;
		} else {
			if (TYPE(cur) == SYMBOL) {
				/* Clear out weak reference in interned_symbols if necessary
				 * We'll have to figure out something better in case we add weak references somewhere else */
				struct gc_reverse_lookup_context context = { NULL, NULL };
				context.value = cur;
				hashtab_foreach(&interned_symbols, gc_reverse_hashtab_lookup, &context);
				if (context.key) {
					hashtab_del(&interned_symbols, context.key);
				}
			}
			free(cur);
#ifdef GC_STATS
			++gc_total_frees;
#endif
		}
	}
	all_allocations_end = writeptr;

#ifdef GC_STATS
	end = gettime_perf();
	time_sweeping += end - start;
#endif

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
	uintptr_t retaddr = (uintptr_t)ret;
	add_allocation(retaddr);
	ret->type = typ;
#ifdef GC_STATS
	++gc_total_allocs;
#endif

	return ret;
}
