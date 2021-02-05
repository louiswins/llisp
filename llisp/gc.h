#pragma once
#include <stdlib.h>
#include "obj.h"
struct contn;
struct env;

/* GC roots */
extern struct contn *gc_current_contn;
extern struct obj_union *gc_current_obj;
extern struct env *gc_global_env;

/* Allocate an object of type `typ' and size `size' */
struct obj *gc_alloc(enum objtype typ, size_t size);
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
