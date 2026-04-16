#include <stdio.h>
#include "cps.h"
#include "parse.h"
#include "stdlib.h"

void add_stdlib(struct env *env) {
	FILE *stdlib_fp = fopen("stdlib.llisp", "r");
	if (!stdlib_fp) {
		fputs("Warning: unable to locate standard library\n", stderr);
		return;
	}
	struct data_source stdlib;
	data_source_from_file(stdlib_fp, &stdlib);

	init_parser();
	struct obj *obj;
	while ((obj = parse(&stdlib)) != NULL) {
		run_cps(obj, env);
	}

	fclose(stdlib_fp);
}