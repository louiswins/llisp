#pragma once
#include <stdlib.h>
struct contn;
struct env;
struct obj;

/* GC roots */
extern struct contn *gc_current_contn;
extern struct obj *gc_current_obj;
extern struct env *gc_global_env;

enum gctype { GC_OBJ, GC_CONTN, GC_ENV, GC_STR, GC_HTENTRY };

struct gc_head {
	struct gc_head *next;
	struct gc_head *marknext;
	enum gctype type;
	_Bool marked;
};
#define NO_GC_HEAD { NULL, NULL, GC_OBJ, 0 }

/* Allocate an object of type `typ' and size `size' */
void *gc_alloc(enum gctype typ, size_t size);
/* Assert that every allocated object is reachable from the three
 * roots above. This is cheap - in particular, it doesn't collect
 * garbage. */
void gc_cycle();
/* Manually collect garbage. */
void gc_collect();

/* GC will not collect while suspended. Make sure everything is
 * reachable from one of the GC roots once you resume. */
void gc_suspend();
void gc_resume();

#ifdef GC_STATS
extern unsigned long long gc_total_allocs;
extern unsigned long long gc_total_frees;
#endif
