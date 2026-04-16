#include <stdio.h>
#include <string.h>
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
	(void)self;
	repl_done = 1;
	*ret = &cend;
	return obj;
}

void repl(struct env *globals) {
	definesym(globals, str_from_string_lit("quit"), make_fn(FN, fn_quit, "quit"));

	init_parser();
	printf("$ ");
	fflush(stdout);
	struct obj *obj;

	struct data_source stdin_ds;
	data_source_from_file(stdin, &stdin_ds);

	while (!repl_done && parse(&stdin_ds, &obj) == PARSE_OK) {
		_Bool failed = 0;
		FILE *output = stdout;
		obj = run_cps(obj, globals, &failed);
		if (failed) {
			printf(" failed with ");
			output = stderr;
		} else {
			printf("=> ");
		}
		if (obj) {
			print_on(output, obj, 1);
		} else {
			fprintf(output, "NULL");
		}
		gc_collect();
		if (!repl_done) {
			printf("\n\n$ ");
			fflush(stdout);
		}
	}
}

_declspec(noinline)
int realmain() {
	struct env *globals = make_env(NULL);
	add_globals(globals);
	add_stdlib(globals);

	repl(globals);

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
