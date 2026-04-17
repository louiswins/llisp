#pragma once
#include <stdlib.h>
#include "obj.h"
struct contn;
struct env;

/* GC roots */
extern struct contn *gc_current_contn;
extern struct obj *gc_current_obj;
extern struct env *gc_global_env;

void gc_init(void *bottom_of_stack);

/* Allocate an object of type `typ' and size `size' */
struct obj *gc_alloc(enum objtype typ, size_t size);
/* Manually collect garbage. */
void gc_collect();

#ifdef GC_STATS
extern unsigned long long gc_total_allocs;
extern unsigned long long gc_total_frees;
extern double time_rootfinding;
extern double time_marking;
extern double time_sweeping;
#endif
