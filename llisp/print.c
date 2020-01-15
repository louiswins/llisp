#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "obj.h"
#include "print.h"

#define MAX_RECURSION 40

void print(struct obj *obj) { print_on(stdout, obj, 1, 0); }
void display(struct obj *obj) { print_on(stdout, obj, 0, 0); }

void print_on(FILE *f, struct obj *obj, int verbose, int reclvl) {
	if (reclvl > MAX_RECURSION) {
		fprintf(f, "...");
		return;
	}
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
			putc('"', f);
			print_str_escaped(f, obj->str);
			putc('"', f);
		} else {
			print_str(f, obj->str);
		}
		break;
	case FN:
		fputs("<#fn>", f);
		break;
	case SPECFORM:
		fputs("<#specform>", f);
		break;
	case LAMBDA:
		fputs("<#closure args=", f);
		print_on(f, obj->args, verbose, reclvl + 1);
		putc('>', f);
		break;
	case MACRO:
		fputs("<#macro args=", f);
		print_on(f, obj->args, verbose, reclvl + 1);
		putc('>', f);
		break;
	case BUILTIN:
		fputs(obj->builtin, f);
		break;
	case CELL: {
		struct obj* tortoise = obj;
		char prev = '(';
		do {
			if (TYPE(obj->tail) != CELL) break;
			putc(prev, f);
			prev = ' ';
			print_on(f, obj->head, verbose, reclvl + 1);
			obj = obj->tail;
			if (TYPE(obj->tail) != CELL) break;
			putc(' ', f);
			print_on(f, obj->head, verbose, reclvl + 1);
			obj = obj->tail;
			tortoise = tortoise->tail;
		} while (tortoise != obj);
		// We check prev to avoid mistaking single dotted pairs for infinite lists
		if (prev == ' ' && tortoise == obj) {
			fprintf(f, " ...)");
			break;
		}
		putc(prev, f);
		print_on(f, obj->head, verbose, reclvl + 1);
		if (obj->tail != &nil) {
			fprintf(f, " . ");
			print_on(f, obj->tail, verbose, reclvl + 1);
		}
		putc(')', f);
		break;
	}
	case CONTN:
		fputs("<#continuation>", f);
		break;
	}
}