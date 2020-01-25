#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "cps.h"
#include "env.h"
#include "gc.h"
#include "globals.h"
#include "obj.h"
#include "print.h"

int length(struct obj *obj) {
	int ret = 0;
	struct obj* tortoise = obj;
	do {
		if (TYPE(obj) != CELL) break;
		obj = obj->tail;
		++ret;
		if (TYPE(obj) != CELL) break;
		obj = obj->tail;
		++ret;
		tortoise = tortoise->tail;
	} while (tortoise != obj);
	if (obj != &nil) return -1;
	return ret;
}
int check_args(const char *fn, struct obj *obj, int nargs) {
	int len = length(obj);
	if (len < 0) {
		fprintf(stderr, "%s: args must be a proper list\n", fn);
		return 0;
	}
	if (len != nargs) {
		fprintf(stderr, "%s: expected %d args, got %d\n", fn, nargs, len);
		return 0;
	}
	return 1;
}

/* (if cond then . else) */
static struct obj *fn_if(CPS_ARGS);
static struct obj *resumeif(CPS_ARGS);

static struct obj *fn_if(CPS_ARGS) {
	int len = length(obj);
	if (len < 0) {
		fputs("if: args must be a proper list", stderr);
		*ret = &cfail;
		return &nil;
	}
	if (len < 2 || len > 3) {
		fprintf(stderr, "if: expected 2 or 3 args, got %d\n", len);
		*ret = &cfail;
		return &nil;
	}
	struct contn *resume = dupcontn(self);
	resume->data = obj->tail;
	resume->fn = resumeif;

	*ret = dupcontn(self);
	(*ret)->next = resume;
	(*ret)->fn = eval_cps;
	return obj->head;
}

/* obj = eval(cond), self->data = (then) or (then else) */
static struct obj *resumeif(CPS_ARGS) {
	if (obj != &false_) {
		/* eval then */
		*ret = dupcontn(self);
		(*ret)->data = &nil;
		(*ret)->fn = eval_cps;
		return self->data->head;
	} else if (self->data->tail != &nil) {
		/* eval else */
		*ret = dupcontn(self);
		(*ret)->data = &nil;
		(*ret)->fn = eval_cps;
		return self->data->tail->head;
	} else {
		/* cond was false, no else. Return false */
		*ret = self->next;
		return &false_;
	}
}

static struct obj *fn_quote(CPS_ARGS) {
	if (!check_args("quote", obj, 1)) {
		*ret = &cfail;
		return &nil;
	}
	*ret = self->next;
	return obj->head;
}

static struct obj *set_symbol_cps(const char *name, struct obj *(*next)(CPS_ARGS), CPS_ARGS) {
	if (!check_args(name, obj, 2)) {
		*ret = &cfail;
		return &nil;
	}
	if (TYPE(obj->head) != SYMBOL) {
		fprintf(stderr, "%s: must define a symbol\n", name);
		*ret = &cfail;
		return &nil;
	}
	struct contn *resume = dupcontn(self);
	resume->data = obj->head;
	resume->fn = next;

	*ret = dupcontn(self);
	(*ret)->next = resume;
	(*ret)->fn = eval_cps;
	return obj->tail->head;
}

static struct obj *fn_define(CPS_ARGS);
static struct obj *do_definesym(CPS_ARGS);

static struct obj *fn_define(CPS_ARGS) {
	return set_symbol_cps("define", do_definesym, self, obj, ret);
}
/* obj = eval(defn), self->data = sym */
static struct obj *do_definesym(CPS_ARGS) {
	definesym(self->env, self->data->str, obj);
	*ret = self->next;
	return &nil;
}

static struct obj *fn_set_(CPS_ARGS);
static struct obj *do_setsym(CPS_ARGS);

static struct obj *fn_set_(CPS_ARGS) {
	return set_symbol_cps("set!", do_setsym, self, obj, ret);
}
/* obj = eval(defn), self->data = sym */
static struct obj *do_setsym(CPS_ARGS) {
	if (!setsym(self->env, self->data->str, obj)) {
		fputs("set!: symbol \"", stderr);
		print_str_escaped(stderr, self->data->str);
		fputs("\" does not exist\n", stderr);
		*ret = &cfail;
		return &nil;
	}
	*ret = self->next;
	return &nil;
}

static struct obj *fn_set_cell(const char* name, void (*actually_set)(struct obj *cell, struct obj *value), CPS_ARGS) {
	if (!check_args(name, obj, 2)) {
		*ret = &cfail;
		return &nil;
	}
	if (TYPE(obj->head) != CELL) {
		fprintf(stderr, "%s: object not a pair\n", name);
		*ret = &cfail;
		return &nil;
	}
	actually_set(obj->head, obj->tail->head);
	*ret = self->next;
	return &nil;
}

static void do_set_car(struct obj *cell, struct obj *value) {
	cell->head = value;
}
static struct obj *fn_set_car_(CPS_ARGS) {
	return fn_set_cell("set-car!", do_set_car, self, obj, ret);
}

static void do_set_cdr(struct obj *cell, struct obj *value) {
	cell->tail = value;
}
static struct obj *fn_set_cdr_(CPS_ARGS) {
	return fn_set_cell("set-cdr!", do_set_cdr, self, obj, ret);
}

static struct obj *fn_car(CPS_ARGS) {
	if (!check_args("car", obj, 1)) {
		*ret = &cfail;
		return &nil;
	}
	if (TYPE(obj->head) != CELL) {
		fputs("car: object not a pair\n", stderr);
		*ret = &cfail;
		return &nil;
	}
	*ret = self->next;
	return obj->head->head;
}
static struct obj *fn_cdr(CPS_ARGS) {
	if (!check_args("cdr", obj, 1)) {
		*ret = &cfail;
		return &nil;
	}
	if (TYPE(obj->head) != CELL) {
		fputs("cdr: object not a pair\n", stderr);
		*ret = &cfail;
		return &nil;
	}
	*ret = self->next;
	return obj->head->tail;
}
static struct obj *fn_cons(CPS_ARGS) {
	if (!check_args("cons", obj, 2)) {
		*ret = &cfail;
		return &nil;
	}
	*ret = self->next;
	return cons(obj->head, obj->tail->head);
}

static struct obj *fn_pair_(CPS_ARGS) {
	if (!check_args("pair?", obj, 1)) {
		*ret = &cfail;
		return &nil;
	}
	*ret = self->next;
	return TYPE(obj->head) == CELL ? &true_ : &false_;
}

static struct obj *fn_begin(CPS_ARGS) {
	*ret = self->next;
	struct obj *val = &nil;
	for (; TYPE(obj) == CELL; obj = obj->tail) {
		val = obj->head;
	}
	return val;
}

static struct obj *fn_gensym(CPS_ARGS) {
	static int symnum = 0;
	int len = length(obj);
	if (len == 1) {
		fputs("gensym: custom prefix not yet implemented, using \" gensym\"\n", stderr);
		len = 0;
	}
	if (len == 0) {
		static const char symformat[] = " gensym%d";
		static size_t maxsymsize = 0;
		if (!maxsymsize) maxsymsize = snprintf(NULL, 0, symformat, INT_MAX) + 1; /* +1 because snprintf always writes a '\0' */

		struct string *s = make_str_cap(maxsymsize);
		s->len = snprintf(s->str, s->cap, symformat, ++symnum); /* No +1 because we don't want the '\0' */
		*ret = self->next;
		return make_symbol(s);
	} else {
		fputs("gensym: expected 0 or 1 args\n", stderr);
		*ret = &cfail;
		return &nil;
	}
}

static struct obj *fn_eq_(CPS_ARGS) {
	if (!check_args("eq?", obj, 2)) {
		*ret = &cfail;
		return &nil;
	}
	*ret = self->next;
	struct obj *a = obj->head;
	struct obj *b = obj->tail->head;
	if (TYPE(a) == NUM && TYPE(b) == NUM) {
		return a->num == b->num ? &true_ : &false_;
	}
	if ((TYPE(a) == SYMBOL && TYPE(b) == SYMBOL) || (TYPE(a) == STRING && TYPE(b) == STRING)) {
		return stringeq(a->str, b->str) ? &true_ : &false_;
	}
	return a == b ? &true_ : &false_;
}

static struct obj *fn_display(CPS_ARGS) {
	if (!check_args("display", obj, 1)) {
		*ret = &cfail;
		return &nil;
	}
	display(obj->head);
	*ret = self->next;
	return &nil;
}
static struct obj *fn_write(CPS_ARGS) {
	if (!check_args("write", obj, 1)) {
		*ret = &cfail;
		return &nil;
	}
	print(obj->head);
	*ret = self->next;
	return &nil;
}
static struct obj *fn_newline(CPS_ARGS) {
	if (!check_args("newline", obj, 0)) {
		*ret = &cfail;
		return &nil;
	}
	putchar('\n');
	*ret = self->next;
	return &nil;
}

static struct obj *fn_callcc(CPS_ARGS) {
	if (!check_args("call-with-current-continuation", obj, 1)) {
		*ret = &cfail;
		return &nil;
	}
	*ret = dupcontn(self);
	(*ret)->fn = eval_cps;
	struct obj *contp = make_obj(CONTN);
	contp->contnp = self->next;
	return cons(obj->head, cons(contp, &nil));
}

static struct obj *fn_error(CPS_ARGS) {
	(void)self;
	*ret = &cfail;
	return obj;
}

static struct obj *fn_apply(CPS_ARGS) {
	if (!check_args("apply", obj, 2)) {
		*ret = &cfail;
		return &nil;
	}
	struct obj* fun = obj->head;
	*ret = dupcontn(self);
	if (TYPE(fun) == CONTN) {
		(*ret)->data = fun;
		(*ret)->fn = apply_contn;
	} else if (TYPE(fun) == FN || TYPE(fun) == SPECFORM) {
		/* Just call the function */
		(*ret)->data = &nil;
		(*ret)->fn = fun->fn;
	} else /* lambda or macro */ {
		(*ret)->data = fun;
		(*ret)->fn = apply_closure;
	}
	return obj->tail->head;
}

static struct obj *make_closure(const char *name, enum objtype type, CPS_ARGS) {
	if (obj == &nil) {
		fprintf(stderr, "%s: must have args\n", name);
		*ret = &cfail;
		return &nil;
	}
	struct obj *args = obj->head;
	if (args != &nil && TYPE(args) != SYMBOL && TYPE(args) != CELL) {
		fprintf(stderr, "%s: expected symbol or list of symbols\n", name);
		*ret = &cfail;
		return &nil;
	}
	if (TYPE(args) == CELL) {
		for (; TYPE(args) == CELL; args = args->tail) {
			if (TYPE(args->head) != SYMBOL) {
				fprintf(stderr, "%s: expected symbol or list of symbols\n", name);
				*ret = &cfail;
				return &nil;
			}
		}
		if (args != &nil && TYPE(args) != SYMBOL) {
			fprintf(stderr, "%s: expected symbol or list of symbols\n", name);
			*ret = &cfail;
			return &nil;
		}
	}
	if (TYPE(obj->tail) != CELL) {
		fprintf(stderr, "%s: invalid body\n", name);
		*ret = &cfail;
		return &nil;
	}
	*ret = self->next;
	struct obj *closure = make_obj(type);
	closure->args = obj->head;
	closure->code = obj->tail;
	closure->env = self->env;
	return closure;
}

static struct obj *fn_lambda(CPS_ARGS) {
	return locate_obj(make_closure("lambda", LAMBDA, self, obj, ret), obj->lineno, obj->pos);
}
static struct obj *fn_macro(CPS_ARGS) {
	return locate_obj(make_closure("macro", MACRO, self, obj, ret), obj->lineno, obj->pos);
}

#define ARITH_OPS(arith) \
	arith(fn_plus, +) \
	arith(fn_minus, -) \
	arith(fn_times, *) \
	arith(fn_div, /, if(obj->head->num==0.){fputs("Warning: /: divide by zero\n", stderr);})
#define NONNUM(arg, op) \
	if (TYPE(arg) != NUM) { \
		fputs(#op ": argument not a number\n", stderr); \
		*ret = &cfail; \
		return &nil; \
	}
#define ARITH_FN(name, op, ...) \
static struct obj *name(CPS_ARGS) { \
	if (obj == &nil) { \
		fputs(#op ": no arguments given\n", stderr); \
		*ret = &cfail; \
		return &nil; \
	} \
	NONNUM(obj->head, op) \
	double val = obj->head->num; \
	for (obj = obj->tail; TYPE(obj) == CELL; obj = obj->tail) { \
		NONNUM(obj->head, op) \
		__VA_ARGS__ \
		val = val op obj->head->num; \
	} \
	if (obj != &nil) { \
		NONNUM(obj, op) \
		__VA_ARGS__ \
		val = val op obj->num; \
	} \
	struct obj *retobj = make_num(val); \
	*ret = self->next; \
	return retobj; \
}
ARITH_OPS(ARITH_FN)
#undef ARITH_FN
#undef NONNUM
static struct obj *fn_mod(CPS_ARGS) {
	if (!check_args("%", obj, 2)) {
		*ret = &cfail;
		return &nil;
	}
	if (TYPE(obj->head) != NUM || TYPE(obj->tail->head) != NUM) {
		fputs("%: argument not a number\n", stderr);
		*ret = &cfail;
		return &nil;
	}
	*ret = self->next;
	return make_num(fmod(obj->head->num, obj->tail->head->num));
}

#define COMPARE_OPS(compare) \
	compare(fn_cmpeq, =, ==) \
	compare(fn_cmplt, <, <) \
	compare(fn_cmpgt, >, >) \
	compare(fn_cmple, <=, <=) \
	compare(fn_cmpge, >=, >=)
#define COMPARE_FN(cname, lispname, op) \
static struct obj *cname(CPS_ARGS) { \
	if (!check_args(#lispname, obj, 2)) { \
		*ret = &cfail; \
		return &nil; \
	} \
	if (TYPE(obj->head) != NUM || TYPE(obj->tail->head) != NUM) { \
		fputs(#lispname ": argument not a number\n", stderr); \
		*ret = &cfail; \
		return &nil; \
	} \
	*ret = self->next; \
	return (obj->head->num op obj->tail->head->num) ? &true_ : &false_; \
}
COMPARE_OPS(COMPARE_FN)
#undef COMPARE_FN

void add_globals(struct env *env) {
#define DEFSYM(name, fn, type) definesym(env, str_from_string_lit(#name), make_fn(type, fn, #name)); gc_cycle()
	DEFSYM(apply, fn_apply, FN);
	DEFSYM(begin, fn_begin, FN);
	DEFSYM(call-with-current-continuation, fn_callcc, FN);
	DEFSYM(car, fn_car, FN);
	DEFSYM(cdr, fn_cdr, FN);
	DEFSYM(cons, fn_cons, FN);
	DEFSYM(define, fn_define, SPECFORM);
	DEFSYM(display, fn_display, FN);
	DEFSYM(eq?, fn_eq_, FN);
	DEFSYM(error, fn_error, FN);
	DEFSYM(gensym, fn_gensym, FN);
	DEFSYM(if, fn_if, SPECFORM);
	DEFSYM(lambda, fn_lambda, SPECFORM);
	DEFSYM(macro, fn_macro, SPECFORM);
	DEFSYM(newline, fn_newline, FN);
	DEFSYM(pair?, fn_pair_, FN);
	DEFSYM(quote, fn_quote, SPECFORM);
	DEFSYM(set!, fn_set_, SPECFORM);
	DEFSYM(set-car!, fn_set_car_, FN);
	DEFSYM(set-cdr!, fn_set_cdr_, FN);
	DEFSYM(write, fn_write, FN);
#define REGISTER_FN(name, op, ...) DEFSYM(op, name, FN);
	ARITH_OPS(REGISTER_FN)
#undef REGISTER_FN
	DEFSYM(%, fn_mod, FN);
#define REGISTER_FN(cname, lispname, ...) DEFSYM(lispname, cname, FN);
	COMPARE_OPS(REGISTER_FN)
#undef REGISTER_FN
#undef DEFSYM
}