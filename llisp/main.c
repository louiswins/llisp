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

void read_line(struct string_builder *current) {
#define BUF_SIZE 1024
	char buf[BUF_SIZE];
	_Bool is_done = 0;
	while (!is_done && fgets(buf, BUF_SIZE, stdin)) {
		size_t real_len = strlen(buf);
		is_done = strchr(buf, '\n') != NULL;
		string_builder_append_str(current, buf, real_len);
	}
}

void repl(struct env *globals) {
	definesym(globals, str_from_string_lit("quit"), make_fn(FN, fn_quit, "quit"));

	printf("$ ");
	fflush(stdout);
	struct obj *obj;

	struct string_builder line;
	struct data_source lineds;

	while (!repl_done) {
		init_string_builder(&line);
		// Keep reading lines until the parse is clean (or fails outright)
		enum parse_result result;
		do {
			read_line(&line);
			data_source_from_memory(line.buf->str, line.used, &lineds);
			result = parse(&lineds, &obj);
		} while (result == PARSE_PARTIAL);

		while (obj != NULL && obj != NIL) {
			_Bool failed = 0;
			FILE *output = stdout;
			struct obj* thisres = run_cps(CAR(obj), globals, &failed);
			if (failed) {
				printf(" failed with ");
				output = stderr;
			} else {
				printf("=> ");
			}
			if (thisres) {
				print_on(output, thisres, 1);
			} else {
				fprintf(output, "NULL");
			}
			puts("");

			obj = CDR(obj);
		}
		gc_collect();
		if (!repl_done) {
			printf("$ ");
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
