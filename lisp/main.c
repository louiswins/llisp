#include <stdio.h>
#include "cps.h"
#include "env.h"
#include "gc.h"
#include "globals.h"
#include "parse.h"
#include "print.h"
#include "stdlib.h"


int main() {
	gc_global_env = make_env(NULL);
	add_globals(gc_global_env);
	add_stdlib(gc_global_env);

	printf("$ ");
	fflush(stdout);
	while ((gc_current_obj = parse_file(stdin)) != NULL) {
		gc_current_obj = run_cps(&gc_current_obj, gc_global_env);
		printf("=> ");print(gc_current_obj);
		printf("\n\n$ ");
		fflush(stdout);
	}
	return 0;
}