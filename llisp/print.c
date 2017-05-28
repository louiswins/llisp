#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "obj.h"
#include "print.h"

void print(struct obj *obj) { print_on(stdout, obj, 1); }
void display(struct obj *obj) { print_on(stdout, obj, 0); }

void print_on(FILE *f, struct obj *obj, int verbose) {
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
		if (verbose) {
			fputc('"', f);
			print_str_escaped(f, obj->str);
			fputc('"', f);
		} else {
			print_str(f, obj->str);
		}
		break;
	case FN:
		fprintf(f, "<#fn>");
		break;
	case SPECFORM:
		fprintf(f, "<#specform>");
		break;
	case LAMBDA:
		fprintf(f, "<#closure args=");
		print_on(f, obj->args, verbose);
		fprintf(f, ">");
		break;
	case MACRO:
		fprintf(f, "<#macro args=");
		print_on(f, obj->args, verbose);
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
			print_on(f, obj->head, verbose);
		}
		putc(prev, f);
		print_on(f, obj->head, verbose);
		if (obj->tail != &nil) {
			fprintf(f, " . ");
			print_on(f, obj->tail, verbose);
		}
		putc(')', f);
	}
	}
}