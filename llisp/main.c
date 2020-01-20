#include <stdio.h>
#include "cps.h"
#include "env.h"
#include "gc.h"
#include "globals.h"
#include "obj.h"
#include "parse.h"
#include "print.h"
#include "stdlib.h"

static int repl_done = 0;
static struct obj *fn_quit(CPS_ARGS) {
	repl_done = 1;
	*ret = self->fail;
	return obj;
}

int main() {
	gc_global_env = make_env(NULL);
	add_globals(gc_global_env);
	add_stdlib(gc_global_env);
	definesym(gc_global_env, str_from_string_lit("quit"), make_fn(FN, fn_quit, "quit"));
	gc_cycle();

	printf("$ ");
	fflush(stdout);
	struct obj *obj;
	while (!repl_done && (obj = parse(stdin)) != NULL) {
		obj = run_cps(obj, gc_global_env);
		printf("=> ");print(obj);
		gc_collect();
		if (!repl_done) {
			printf("\n\n$ ");
			fflush(stdout);
		}
	}

#ifdef GC_STATS
	printf("Total allocations:               %llu\n", gc_total_allocs);
	printf("Total frees (before collection): %llu\n", gc_total_frees);
	gc_global_env = NULL; gc_current_contn = NULL; gc_current_obj = NULL;
	gc_cycle(); gc_collect();
	printf("Total frees (after collection):  %llu\n", gc_total_frees);
	printf("Leaked memory:                   %llu\n", gc_total_allocs - gc_total_frees);
#endif

	return 0;
}