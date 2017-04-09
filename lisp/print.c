#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "env.h"
#include "gc.h"
#include "obj.h"
#include "print.h"

//	enum objtype { CELL, NUM, SYMBOL, FN, SPECFORM, LAMBDA, BUILTIN } type;
static void realprint(FILE *f, struct obj *obj, struct env *env) {
#if 0
	{
		/* debug stuff */
		size_t index = object_indexof(obj);
		if (index != (size_t)-1) {
			hasbeenprinted[object_indexof(obj)] = 1;
		}
		if (env) {
			const char *name = revlookup_debug(env, obj);
			if (name) {
				fprintf(f, "%s:", name);
			}
		}
	}
#endif
	switch (TYPE(obj)) {
	default:
		fprintf(stderr, "<#unknown type %d>", TYPE(obj));
		break;
	case NUM: {
		double d = obj->num;
		if (d == round(d) && d >= INT_MIN && d <= INT_MAX) {
			/* if d is integral, print it as such */
			fprintf(f, "%d", (int)d);
		} else {
			fprintf(f, "%f", obj->num);
		}
		break;
	}
	case SYMBOL:
		print_str(f, obj->sym);
		break;
	case FN:
		fprintf(f, "<#fn>");
		break;
	case SPECFORM:
		fprintf(f, "<#specform>");
		break;
	case LAMBDA:
		fprintf(f, "<#closure args=");
		realprint(f, obj->args, env);
#if 0
		if (env) {
			size_t index = object_indexof(obj->code);
			if (index != (size_t)-1 && !hasbeenprinted[index]) {
				fprintf(f, " code=");
				realprint(f, obj->code, env);
			}
		}
#endif
		fprintf(f, ">");
		break;
	case MACRO:
		fprintf(f, "<#macro args=");
		realprint(f, obj->args, env);
#if 0
		if (env) {
			size_t index = object_indexof(obj->code);
			if (index != (size_t)-1 && !hasbeenprinted[index]) {
				fprintf(f, " code=");
				realprint(f, obj->code, env);
			}
		}
#endif
		fprintf(f, ">");
		break;
	case BUILTIN:
		fprintf(f, "%s", obj->builtin);
		break;
	case CELL: {
		char prev = '(';
		for (; TYPE(obj->tail) == CELL; obj = obj->tail) {
			putc(prev, f);
			prev = ' ';
			realprint(f, obj->head, env);
		}
		putc(prev, f);
		realprint(f, obj->head, env);
		if (obj->tail != &nil) {
			fprintf(f, " . ");
			realprint(f, obj->tail, env);
		}
		putc(')', f);
	}
	}
}

void print(struct obj *obj) { realprint(stdout, obj, NULL); }
void print_debug(struct obj *obj, struct env *env) {
//	memset(hasbeenprinted, 0, MAXOBJECTS);
	realprint(stderr, obj, env);
}