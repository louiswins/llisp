#include <stdio.h>
#include "cps.h"
#include "parse.h"
#include "stdlib.h"

void add_stdlib(struct env *env) {
	FILE *stdlib = fopen("stdlib.llisp", "r");
	if (!stdlib) {
		fputs("Warning: unable to locate standard library\n", stderr);
		return;
	}
	struct obj *obj;
	while ((obj = parse(stdlib)) != NULL) {
		run_cps(obj, env);
	}
	fclose(stdlib);
}