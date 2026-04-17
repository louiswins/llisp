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

_declspec(noinline)
int realmain() {
	struct env *globals = make_env(NULL);
	add_globals(globals);
	add_stdlib(globals);
	definesym(globals, str_from_string_lit("quit"), make_fn(FN, fn_quit, "quit"));

	init_parser();
	printf("$ ");
	fflush(stdout);
	struct obj *obj;
	while (!repl_done && (obj = parse(stdin)) != NULL) {
		obj = run_cps(obj, globals);
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
	memset(&interned_symbols, 0, sizeof(interned_symbols));
	gc_collect();
	printf("Total frees (after collection):  %llu\n", gc_total_frees);
	printf("Leaked memory:                   %llu\n", gc_total_allocs - gc_total_frees);
	printf("Time rootfinding:                %f\n", time_rootfinding);
	printf("Time marking:                    %f\n", time_marking);
	printf("Time sweeping:                   %f\n", time_sweeping);
#endif

	return 0;
}

int main() {
	/* make sure the GC scans everything in main */
	void *bottom_of_stack = &bottom_of_stack;
	gc_init(bottom_of_stack);
	return realmain();
}
