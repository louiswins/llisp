#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "obj.h"
#include "print.h"

void print(struct obj *obj) { print_on(stdout, obj); }

void print_on(FILE *f, struct obj *obj) {
	switch (TYPE(obj)) {
	default:
		fprintf(stderr, "<#unknown type %d>", TYPE(obj));
		break;
	case NUM: {
		double d = obj->num;
		if (d == round(d) && d >= LONG_MIN && d <= LONG_MAX) {
			/* if d is integral, print it as such */
			fprintf(f, "%ld", (long)d);
		} else {
			fprintf(f, "%f", obj->num);
		}
		break;
	}
	case SYMBOL:
		print_str(f, obj->str);
		break;
	case STRING:
		fputc('"', f);
		print_str_escaped(f, obj->str);
		fputc('"', f);
		break;
	case FN:
		fprintf(f, "<#fn>");
		break;
	case SPECFORM:
		fprintf(f, "<#specform>");
		break;
	case LAMBDA:
		fprintf(f, "<#closure args=");
		print_on(f, obj->args);
		fprintf(f, ">");
		break;
	case MACRO:
		fprintf(f, "<#macro args=");
		print_on(f, obj->args);
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
			print_on(f, obj->head);
		}
		putc(prev, f);
		print_on(f, obj->head);
		if (obj->tail != &nil) {
			fprintf(f, " . ");
			print_on(f, obj->tail);
		}
		putc(')', f);
	}
	}
}