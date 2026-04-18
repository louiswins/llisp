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
	struct obj *obj;

	struct string_builder line;
	struct buf linebuf;

	while (!repl_done) {
		init_string_builder(&line);
		// Keep reading lines until the parse is clean (or fails outright)
		enum parse_result result;
		const char *prompt = "$ ";
		do {
			printf(prompt);
			fflush(stdout);
			read_line(&line);
			init_buf(line.buf->str, line.used, &linebuf);
			result = parse(&linebuf, &obj);
			prompt = "| ";
		} while (result == PARSE_PARTIAL);

		while (obj != NULL && obj != NIL) {
			_Bool failed = 0;
			repl_needs_newline = 0;
			struct obj* thisres = run_cps(CAR(obj), globals, &failed);
			if (repl_needs_newline) {
				/* Add a newline so we don't put the => immediately on top of it */
				/* (or the next prompt */
				putchar('\n');
			}
			if (!failed) {
				printf("=> ");

				if (thisres) {
					print(thisres);
					putchar('\n');
				} else {
					puts("NULL");
				}
			}

			obj = CDR(obj);
		}
		gc_collect();
	}
}

void run_file(char *filename, struct env *globals) {
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		fprintf(stderr, "Error opening file \"%s\"\n", filename);
		exit(1);
	}
	fseek(fp, 0, SEEK_END);
	size_t file_size = ftell(fp);
	rewind(fp);

	struct string *file_contents = unsafe_make_uninitialized_str(file_size);
	size_t bytes_read = 0;
	while (bytes_read < file_size) {
		size_t this_read = fread(file_contents->str + bytes_read, 1, file_size - bytes_read, fp);
		if (this_read == 0) {
			fprintf(stderr, "Error reading file \"%s\"\n", filename);
			exit(1);
		}
		bytes_read += this_read;
	}
	fclose(fp);

	struct buf buf;
	init_buf(file_contents->str, file_contents->len, &buf);

	struct obj *obj;
	enum parse_result parse_res = parse(&buf, &obj);
	if (parse_res == PARSE_EMPTY) {
		/* I don't know why you'd do this, but I guess it's fine */
		return;
	}
	if (parse_res != PARSE_OK) {
		fprintf(stderr, "Syntax error in \"%s\"\n", filename);
		exit(1);
	}

	/* parsed successfully - run it */
	while (obj != NIL) {
		run_cps(CAR(obj), globals, NULL /*failed*/);
		obj = CDR(obj);
	}
}

_declspec(noinline)
int realmain(int argc, char *argv[]) {
	struct env *globals = make_env(NULL);
	add_globals(globals);
	add_stdlib(globals);

	if (argc == 1) {
		repl(globals);
	} else if (argc == 2) {
		run_file(argv[1], globals);
	} else {
		fprintf(stderr, "Usage: %s [file]\n", argv[0]);
		return 1;
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

int main(int argc, char *argv[]) {
	/* make sure the GC scans everything in main */
	void *bottom_of_stack = &bottom_of_stack;
	gc_init(bottom_of_stack);
	return realmain(argc, argv);
}
