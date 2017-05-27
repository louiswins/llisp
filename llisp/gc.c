#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "cps.h"
#include "env-private.h"
#include "gc.h"
#include "obj.h"

struct gc_head {
	struct gc_head *next;
	uintptr_t marknext;
};

#define ISMARKED(o) ((o)->marknext & 0x1u)
#define ADDMARK(o) ((o)->marknext |= 0x1u)
#define DELMARK(o) ((o)->marknext &= ~0x1ull)
#define NEXTTOMARK(o) ((struct gc_head*)((o)->marknext & ~0x7ull))
#define SETNEXTTOMARK(o, val) ((o)->marknext = (((o)->marknext & 0x7u) | (uintptr_t)val))

#define GCTYPE(o) ((enum gctype)(((o)->marknext & 0x6u) >> 1))

#define GC_FROM_OBJ(o) ((struct gc_head*)((char*)(o) - SIZE_OF_HEAD))
#define OBJ_FROM_GC(gc) ((void*)((char*)(gc) + SIZE_OF_HEAD))

#define ALLOC_ALIGN 0x10
#define SIZE_OF_HEAD ((sizeof(struct gc_head) + ALLOC_ALIGN - 1) & ~(ALLOC_ALIGN - 1))

typedef char assert_alignof_obj_ok[__alignof(struct obj) <= ALLOC_ALIGN ? 1 : -1];
typedef char assert_alignof_env_ok[__alignof(struct env) <= ALLOC_ALIGN ? 1 : -1];
typedef char assert_alignof_contn_ok[__alignof(struct contn) <= ALLOC_ALIGN ? 1 : -1];
typedef char assert_room_for_tagbits[3 < __alignof(void*) ? 1 : -1];

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
static void gc_mark(struct gc_head *gcitem) {
	if (ISMARKED(gcitem)) return;
	ADDMARK(gcitem);
	if (GCTYPE(gcitem) == GC_STR) return; /* no pointers in a string :) */
	if (GCTYPE(gcitem) == GC_ENV) {
		struct env *env = OBJ_FROM_GC(gcitem);
		gc_queue(env->next);
		gc_queue(env->parent);
		for (int i = 0; i < env->nsyms; ++i) {
			gc_queue(env->syms[i].name);
			gc_queue(env->syms[i].value);
		}
		return;
	}
	if (GCTYPE(gcitem) == GC_CONTN) {
		struct contn *contn = OBJ_FROM_GC(gcitem);
		gc_queue(contn->data);
		gc_queue(contn->env);
		gc_queue(contn->next);
		gc_queue(contn->fail);
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
		gc_queue(obj->sym);
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
		return;
	case CONTN:
		gc_queue(obj->contnp);
		return;
	}
}

static void clear_marks() {
	for (struct gc_head *cur = all_objects; cur != NULL; cur = cur->next) {
		assert(!ISMARKED(cur));
		DELMARK(cur);
	}
}

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
			free(leaked);
#ifdef GC_STATS
			++gc_total_frees;
#endif
		}
	}
}

void *gc_alloc(enum gctype typ, size_t size) {
	size = (size - 1 + ALLOC_ALIGN) & ~(ALLOC_ALIGN - 1);
#ifdef DEBUG_GC
	static unsigned char num_allocs = 0;
	if (!++num_allocs) { gc_collect(); }
#endif
	struct gc_head *ret = malloc(SIZE_OF_HEAD + size);
	if (ret == NULL) {
		gc_collect();
		ret = malloc(SIZE_OF_HEAD + size);
		if (ret == NULL) {
			fputs("Out of memory\n", stderr);
			abort();
		}
	}
	ret->next = all_objects;
	ret->marknext = (uintptr_t)typ << 1;
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