#include <stdio.h>
#include <string.h>
#include "cps.h"
#include "env.h"
#include "gc.h"
#include "globals.h"
#include "macroexpander.h"
#include "obj.h"
#include "parse.h"
#include "print.h"
#include "stdlib.h"

static int repl_done = 0;
static struct obj *fn_quit(CPS_ARGS) {
	(void)self;
	repl_done = 1;
	*ret = &cfail;
	return obj;
}

int main() {
	gc_global_env = make_env(NULL);
	add_globals(gc_global_env);
	add_stdlib(gc_global_env);
	definesym(gc_global_env, str_from_string_lit("quit"), make_fn(FN, fn_quit, "quit"));
	gc_cycle();

	init_parser();
	printf("$ ");
	fflush(stdout);
	struct obj *obj;
	while (!repl_done && (obj = parse(stdin)) != NULL) {
		obj = run_cps(obj, gc_global_env);
		printf("=> ");
		if (obj) {
			print(obj);
		} else {
			printf("NULL");
		}
		gc_collect();
		if (!repl_done) {
			printf("\n\n$ ");
			fflush(stdout);
		}
	}

#ifdef GC_STATS
	puts("\n");
	printf("Total allocations:               %llu\n", gc_total_allocs);
	printf("Total frees (before collection): %llu\n", gc_total_frees);
	gc_global_env = NULL; gc_current_contn = NULL; gc_current_obj = NULL;
	memset(&interned_symbols, 0, sizeof(interned_symbols));
	gc_cycle(); gc_collect();
	printf("Total frees (after collection):  %llu\n", gc_total_frees);
	printf("Leaked memory:                   %llu\n", gc_total_allocs - gc_total_frees);
#endif

	return 0;
}
