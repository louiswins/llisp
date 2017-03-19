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
	struct obj *obj;
	while ((obj = parse_file(stdin)) != NULL) {
		obj = run_cps(obj, gc_global_env);
		printf("=> ");print(obj);
		printf("\n\n$ ");
		fflush(stdout);
	}
	return 0;
}