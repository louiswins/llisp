#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "gc.h"

#include "cps.h"
#include "env-private.h"
#include "obj.h"

struct thing_to_alloc {
	struct gc_head h;
	union {
		struct obj a;
		struct env b;
		struct contn c;
	} u;
};

#define ISMARKED(o) ((o)->marknext & 0x1u)
#define ADDMARK(o) ((o)->marknext |= 0x1u)
#define DELMARK(o) ((o)->marknext &= ~0x1ull)
#define NEXTTOMARK(o) ((struct gc_head*)((o)->marknext & ~0x1ull))
#define SETNEXTTOMARK(o, val) ((o)->marknext = (ISMARKED(o) | (uintptr_t)val))
#define GC_FROM_OBJ(o) ((struct gc_head*)(((unsigned char *)o) - offsetof(struct thing_to_alloc, u)))
#define OBJ_FROM_GC(gc) (&(((struct thing_to_alloc*)gc)->u))

/* GC roots */
struct contn *gc_current_contn = NULL;
struct obj *gc_current_obj = NULL;
struct env *gc_global_env = NULL;

static int gc_active = 1;

#define MAX_TEMP_ROOTS 32
static size_t ntempcontns = 0;
static struct contn *temp_contns[MAX_TEMP_ROOTS];
static size_t ntempenvs = 0;
static struct env *temp_envs[MAX_TEMP_ROOTS];

static struct gc_head *all_objects = NULL;
static struct gc_head *objs_to_mark = NULL;

void gc_add_to_temp_contns(struct contn *contn) {
	assert(ntempcontns < MAX_TEMP_ROOTS);
	temp_contns[ntempcontns++] = contn;
}
void gc_add_to_temp_envs(struct env *env) {
	assert(ntempcontns < MAX_TEMP_ROOTS);
	temp_envs[ntempenvs++] = env;
}
void gc_clear_temp_roots() { ntempcontns = ntempenvs = 0; }

static void mark_contn(struct contn *contn);
static void mark_env(struct env *env);
static void mark_obj(struct obj *obj);
static void queue_obj(struct obj *obj);

static void mark_contn(struct contn *contn) {
	if (contn == &cbegin || contn == &cend || contn == &cfail) return;
	if (contn == NULL || ISMARKED(GC_FROM_OBJ(contn))) return;
	ADDMARK(GC_FROM_OBJ(contn));
	queue_obj(contn->data);
	mark_env(contn->env);
	mark_contn(contn->next);
	mark_contn(contn->fail);
}
static void queue_obj(struct obj *obj) {
	/* actually a stack :) */
	if (obj == &nil || obj == &true_ || obj == &false_) return;
	if (obj == NULL || ISMARKED(GC_FROM_OBJ(obj))) return;
	struct gc_head *gc = GC_FROM_OBJ(obj);
	if (NEXTTOMARK(gc) != NULL) return; // already in queue
	SETNEXTTOMARK(gc, objs_to_mark);
	objs_to_mark = gc;
}
static void mark_obj(struct obj *obj) {
	if (ISMARKED(GC_FROM_OBJ(obj))) return;
	ADDMARK(GC_FROM_OBJ(obj));
	switch (TYPE(obj)) {
	case NUM:
	case SYMBOL:
	case FN:
	case SPECFORM:
	case BUILTIN:
		return;
	case CELL:
		queue_obj(obj->head);
		queue_obj(obj->tail);
		return;
	case LAMBDA:
	case MACRO:
		queue_obj(obj->args);
		queue_obj(obj->code);
		mark_env(obj->env);
		return;
	case CONTN:
		mark_contn(obj->contnp);
		return;
	}
}
static void mark_env(struct env *env) {
	if (env == NULL || ISMARKED(GC_FROM_OBJ(env))) return;
	ADDMARK(GC_FROM_OBJ(env));
	for (int i = 0; i < env->nsyms; ++i) {
		queue_obj(env->syms[i].value);
	}
	mark_env(env->next);
	mark_env(env->parent);
}

static void clear_marks() {
	for (struct gc_head *cur = all_objects; cur != NULL; cur = cur->next) {
		assert(!ISMARKED(cur));
		DELMARK(cur);
	}
}

static void gc_collect() {
	if (!gc_active) return;
	clear_marks();

	/* mark */
	mark_contn(gc_current_contn);
	queue_obj(gc_current_obj);
	mark_env(gc_global_env);
	for (size_t i = 0; i < ntempcontns; ++i) {
		mark_contn(temp_contns[i]);
	}
	for (size_t i = 0; i < ntempenvs; ++i) {
		mark_env(temp_envs[i]);
	}
	/* avoid infinite recursion */
	while (objs_to_mark != NULL) {
		struct gc_head *cur = objs_to_mark;
		objs_to_mark = NEXTTOMARK(cur);
		SETNEXTTOMARK(cur, NULL);
		mark_obj((struct obj *)OBJ_FROM_GC(cur));
	}

#pragma warning(push)
#pragma warning(disable: 6001) /* VS doesn't understand that every object in all_objects has been initialized */
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
		}
	}
#pragma warning(pop)
}

unsigned long long n_allocs = 0ull;
unsigned char count = 0;
void *gc_alloc(size_t size) {
	(void)size;
	++n_allocs;
	struct gc_head *ret = malloc(sizeof(struct thing_to_alloc));
	++count;
	if (ret == NULL) {
		gc_collect();
		ret = malloc(sizeof(struct thing_to_alloc));
		if (ret == NULL) {
			fputs("Out of memory\n", stderr);
			abort();
		}
	} else if (!count) {
		gc_collect();
	}
	ret->next = all_objects;
	ret->marknext = 0;
	typedef char assert_NULL_is_0[(uintptr_t)NULL == 0u ? 1 : -1];
	all_objects = ret;
	return OBJ_FROM_GC(ret);
}

void gc_suspend() {
	gc_active = 0;
}
void gc_resume() {
	gc_active = 1;
}