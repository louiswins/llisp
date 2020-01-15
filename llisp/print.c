#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gc-private.h"
#include "obj.h"
#include "print.h"

#define OBJ_MARKED(o) ISMARKED(GC_FROM_OBJ(o))
#define MARK_OBJ(o) ADDMARK(GC_FROM_OBJ(o))
#define DEL_OBJMARK(o) DELMARK(GC_FROM_OBJ(o))

void print(struct obj *obj) { print_on(stdout, obj, 1); }
void display(struct obj *obj) { print_on(stdout, obj, 0); }

static void print_on_helper(FILE *f, struct obj *obj, int verbose) {
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
		print_on_helper(f, obj->args, verbose);
		putc('>', f);
		break;
	case MACRO:
		fputs("<#macro args=", f);
		print_on_helper(f, obj->args, verbose);
		putc('>', f);
		break;
	case BUILTIN:
		fputs(obj->builtin, f);
		break;
	case CELL: {
		// We appropriate the GC marking to avoid printing cyclic objects
		if (OBJ_MARKED(obj)) {
			fprintf(f, "...");
			return;
		}
		char prev = '(';
		for (; TYPE(obj->tail) == CELL; obj = obj->tail) {
			MARK_OBJ(obj);
			putc(prev, f);
			prev = ' ';
			print_on_helper(f, obj->head, verbose);
			if (OBJ_MARKED(obj->tail)) {
				fprintf(f, " ...)");
				return;
			}
		}
		MARK_OBJ(obj);
		putc(prev, f);
		print_on_helper(f, obj->head, verbose);
		if (obj->tail != &nil) {
			fprintf(f, " . ");
			print_on_helper(f, obj->tail, verbose);
		}
		putc(')', f);
		break;
	}
	case CONTN:
		fputs("<#continuation>", f);
		break;
	}
}

static void clear_marks(struct obj *obj) {
	switch (TYPE(obj)) {
	case LAMBDA:
	case MACRO:
		clear_marks(obj->args);
		break;
	case CELL:
		if (OBJ_MARKED(obj)) {
			DEL_OBJMARK(obj);
			clear_marks(obj->head);
			clear_marks(obj->tail);
		}
		break;
	}
}

void print_on(FILE *f, struct obj *obj, int verbose) {
	print_on_helper(f, obj, verbose);
	// Now that we've printed, let's clear all the marks to avoid messing up the GC
	clear_marks(obj);
}
