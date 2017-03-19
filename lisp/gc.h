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

void gc_add_to_temp_contns(struct contn *contn);
void gc_add_to_temp_envs(struct env *env);
void gc_clear_temp_roots();

void *gc_alloc(size_t size);

void gc_suspend();
void gc_resume();