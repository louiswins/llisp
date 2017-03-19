#pragma once
#include <stdint.h>
#include <stdlib.h>
struct contn;
struct env;
struct obj;

struct gc_head {
	struct gc_head *next;
	uintptr_t marknext;
};

/* GC roots */
extern struct contn *gc_current_contn;
extern struct obj *gc_current_obj;
extern struct env *gc_global_env;

enum gctype { GC_OBJ, GC_CONTN, GC_ENV };

void gc_add_to_temp_roots(void *root);
void gc_clear_temp_roots();

void *gc_alloc(enum gctype typ, size_t size);
void gc_collect();

void gc_suspend();
void gc_resume();