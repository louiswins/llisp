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
		double d = AS_NUM(obj);
		if (d == round(d) && d >= LONG_MIN && d <= LONG_MAX) {
			/* if d is integral, print it as such */
			fprintf(f, "%ld", (long)d);
		} else {
			fprintf(f, "%f", AS_NUM(obj));
		}
		break;
	}
	case SYMBOL:
		print_str(f, AS_SYMBOL(obj));
		break;
	case STRING:
		if (verbose) {
			putc('"', f);
			print_str_escaped(f, AS_STRING(obj));
			putc('"', f);
		} else {
			print_str(f, AS_STRING(obj));
		}
		break;
	case FN:
		fprintf(f, "<#fn %s>", AS_FN(obj)->fnname);
		break;
	case SPECFORM:
		fprintf(f, "<#specform %s>", AS_FN(obj)->fnname);
		break;
	case LAMBDA:
		fputs("<#closure ", f);
		if (AS_CLOSURE(obj)->closurename) {
			print_str(f, AS_CLOSURE(obj)->closurename);
			fputc(' ', f);
		}
		fputs("args=", f);
		print_on_helper(f, AS_CLOSURE(obj)->args, verbose);
		putc('>', f);
		break;
	case MACRO:
		fputs("<#macro ", f);
		if (AS_CLOSURE(obj)->closurename) {
			print_str(f, AS_CLOSURE(obj)->closurename);
			fputc(' ', f);
		}
		fputs("args=", f);
		print_on_helper(f, AS_CLOSURE(obj)->args, verbose);
		putc('>', f);
		break;
	case BUILTIN:
		fputs(AS_BUILTIN(obj)->builtin, f);
		break;
	case CELL: {
		// We appropriate the GC marking to avoid printing cyclic objects
		if (OBJ_MARKED(obj)) {
			fprintf(f, "...");
			return;
		}
		char prev = '(';
		for (; TYPE(CDR(obj)) == CELL; obj = CDR(obj)) {
			MARK_OBJ(obj);
			putc(prev, f);
			prev = ' ';
			print_on_helper(f, CAR(obj), verbose);
			if (OBJ_MARKED(CDR(obj))) {
				fprintf(f, " ...)");
				return;
			}
		}
		MARK_OBJ(obj);
		putc(prev, f);
		print_on_helper(f, CAR(obj), verbose);
		if (CDR(obj) != NIL) {
			fprintf(f, " . ");
			print_on_helper(f, CDR(obj), verbose);
		}
		putc(')', f);
		break;
	}
	case OBJ_CONTN:
		fputs("<#continuation>", f);
		break;
	}
}

static void clear_marks(struct obj *obj) {
	switch (TYPE(obj)) {
	case LAMBDA:
	case MACRO:
		clear_marks(AS_CLOSURE(obj)->args);
		break;
	case CELL:
		if (OBJ_MARKED(obj)) {
			DEL_OBJMARK(obj);
			clear_marks(CAR(obj));
			clear_marks(CDR(obj));
		}
		break;
	}
}

void print_on(FILE *f, struct obj *obj, int verbose) {
	print_on_helper(f, obj, verbose);
	// Now that we've printed, let's clear all the marks to avoid messing up the GC
	clear_marks(obj);
}
