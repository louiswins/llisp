#include <assert.h>
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
		obj = CDR(obj);
		++ret;
		if (TYPE(obj) != CELL) break;
		obj = CDR(obj);
		++ret;
		tortoise = CDR(tortoise);
	} while (tortoise != obj);
	if (obj != NIL) return -1;
	return ret;
}
_Bool check_args(const char *fn, struct obj *obj, int nargs) {
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
		return NIL;
	}
	if (len < 2 || len > 3) {
		fprintf(stderr, "if: expected 2 or 3 args, got %d\n", len);
		*ret = &cfail;
		return NIL;
	}
	struct contn *resume = dupcontn(self);
	resume->data = CDR(obj);
	resume->fn = resumeif;

	*ret = dupcontn(self);
	(*ret)->next = resume;
	(*ret)->fn = eval_cps;
	return CAR(obj);
}

/* obj = eval(cond), self->data = (then) or (then else) */
static struct obj *resumeif(CPS_ARGS) {
	if (obj != FALSE) {
		/* eval then */
		*ret = dupcontn(self);
		(*ret)->data = NIL;
		(*ret)->fn = eval_cps;
		return CAR(self->data);
	} else if (CDR(self->data) != NIL) {
		/* eval else */
		*ret = dupcontn(self);
		(*ret)->data = NIL;
		(*ret)->fn = eval_cps;
		return CAR(CDR(self->data));
	} else {
		/* cond was false, no else. Return false */
		*ret = self->next;
		return FALSE;
	}
}

static struct obj *fn_quote(CPS_ARGS) {
	if (!check_args("quote", obj, 1)) {
		*ret = &cfail;
		return NIL;
	}
	*ret = self->next;
	return CAR(obj);
}

static struct obj *set_symbol_cps(const char *name, struct obj *(*next)(CPS_ARGS), CPS_ARGS) {
	if (!check_args(name, obj, 2)) {
		*ret = &cfail;
		return NIL;
	}
	if (TYPE(CAR(obj)) != SYMBOL) {
		fprintf(stderr, "%s: must define a symbol\n", name);
		*ret = &cfail;
		return NIL;
	}
	struct contn *resume = dupcontn(self);
	resume->data = CAR(obj);
	resume->fn = next;

	*ret = dupcontn(self);
	(*ret)->next = resume;
	(*ret)->fn = eval_cps;
	return CAR(CDR(obj));
}

static struct obj *fn_define(CPS_ARGS);
static struct obj *do_definesym(CPS_ARGS);

static struct obj *fn_define(CPS_ARGS) {
	return set_symbol_cps("define", do_definesym, self, obj, ret);
}
/* obj = eval(defn), self->data = sym */
static struct obj *do_definesym(CPS_ARGS) {
	definesym(self->env, AS_SYMBOL(self->data), obj);
	*ret = self->next;
	return NIL;
}

static struct obj *fn_set_(CPS_ARGS);
static struct obj *do_setsym(CPS_ARGS);

static struct obj *fn_set_(CPS_ARGS) {
	return set_symbol_cps("set!", do_setsym, self, obj, ret);
}
/* obj = eval(defn), self->data = sym */
static struct obj *do_setsym(CPS_ARGS) {
	if (!setsym(self->env, AS_SYMBOL(self->data), obj)) {
		fputs("set!: symbol \"", stderr);
		print_str_escaped(stderr, AS_SYMBOL(self->data));
		fputs("\" does not exist\n", stderr);
		*ret = &cfail;
		return NIL;
	}
	*ret = self->next;
	return NIL;
}

static struct obj *fn_set_cell(const char* name, void (*actually_set)(struct obj *cell, struct obj *value), CPS_ARGS) {
	if (!check_args(name, obj, 2)) {
		*ret = &cfail;
		return NIL;
	}
	if (TYPE(CAR(obj)) != CELL) {
		fprintf(stderr, "%s: object not a pair\n", name);
		*ret = &cfail;
		return NIL;
	}
	actually_set(CAR(obj), CAR(CDR(obj)));
	*ret = self->next;
	return NIL;
}

static void do_set_car(struct obj *cell, struct obj *value) {
	CAR(cell) = value;
}
static struct obj *fn_set_car_(CPS_ARGS) {
	return fn_set_cell("set-car!", do_set_car, self, obj, ret);
}

static void do_set_cdr(struct obj *cell, struct obj *value) {
	CDR(cell) = value;
}
static struct obj *fn_set_cdr_(CPS_ARGS) {
	return fn_set_cell("set-cdr!", do_set_cdr, self, obj, ret);
}

static struct obj *fn_car(CPS_ARGS) {
	if (!check_args("car", obj, 1)) {
		*ret = &cfail;
		return NIL;
	}
	if (TYPE(CAR(obj)) != CELL) {
		fputs("car: object not a pair\n", stderr);
		*ret = &cfail;
		return NIL;
	}
	*ret = self->next;
	return CAR(CAR(obj));
}
static struct obj *fn_cdr(CPS_ARGS) {
	if (!check_args("cdr", obj, 1)) {
		*ret = &cfail;
		return NIL;
	}
	if (TYPE(CAR(obj)) != CELL) {
		fputs("cdr: object not a pair\n", stderr);
		*ret = &cfail;
		return NIL;
	}
	*ret = self->next;
	return CDR(CAR(obj));
}
static struct obj *fn_cons(CPS_ARGS) {
	if (!check_args("cons", obj, 2)) {
		*ret = &cfail;
		return NIL;
	}
	*ret = self->next;
	return cons(CAR(obj), CAR(CDR(obj)));
}

static struct obj *fn_pair_(CPS_ARGS) {
	if (!check_args("pair?", obj, 1)) {
		*ret = &cfail;
		return NIL;
	}
	*ret = self->next;
	return TYPE(CAR(obj)) == CELL ? TRUE : FALSE;
}

static struct obj *fn_begin(CPS_ARGS) {
	*ret = self->next;
	struct obj *val = NIL;
	for (; TYPE(obj) == CELL; obj = CDR(obj)) {
		val = CAR(obj);
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
	if (len) {
		fputs("gensym: expected 0 or 1 args\n", stderr);
		*ret = &cfail;
		return NIL;
	}

	char buf[32];
	int slen = snprintf(buf, 32, " gensym%d", ++symnum); /* snprintf returns the actual length without '\0' (although it writes it) */
	assert(slen < 32);
	*ret = self->next;
	/* TODO: consider not interning this symbol? */
	return intern_symbol(make_str_from_ptr_len(buf, slen));
}

static struct obj *fn_eq_(CPS_ARGS) {
	if (!check_args("eq?", obj, 2)) {
		*ret = &cfail;
		return NIL;
	}
	*ret = self->next;
	struct obj *a = CAR(obj);
	struct obj *b = CAR(CDR(obj));
	if (TYPE(a) == NUM && TYPE(b) == NUM) {
		return AS_NUM(a) == AS_NUM(b) ? TRUE : FALSE;
	}
	return a == b ? TRUE : FALSE;
}

static struct obj *fn_display(CPS_ARGS) {
	if (!check_args("display", obj, 1)) {
		*ret = &cfail;
		return NIL;
	}
	display(CAR(obj));
	*ret = self->next;
	return NIL;
}
static struct obj *fn_write(CPS_ARGS) {
	if (!check_args("write", obj, 1)) {
		*ret = &cfail;
		return NIL;
	}
	print(CAR(obj));
	*ret = self->next;
	return NIL;
}
static struct obj *fn_newline(CPS_ARGS) {
	if (!check_args("newline", obj, 0)) {
		*ret = &cfail;
		return NIL;
	}
	putchar('\n');
	*ret = self->next;
	return NIL;
}

static struct obj *fn_callcc(CPS_ARGS) {
	if (!check_args("call-with-current-continuation", obj, 1)) {
		*ret = &cfail;
		return NIL;
	}
	*ret = dupcontn(self);
	(*ret)->fn = eval_cps;
	return cons(CAR(obj), cons((struct obj *) self->next, NIL));
}

static struct obj *fn_error(CPS_ARGS) {
	(void)self;
	*ret = &cfail;
	return obj;
}

static struct obj *fn_apply(CPS_ARGS) {
	if (!check_args("apply", obj, 2)) {
		*ret = &cfail;
		return NIL;
	}
	struct obj* fun = CAR(obj);
	*ret = dupcontn(self);
	if (TYPE(fun) == CONTN) {
		(*ret)->data = fun;
		(*ret)->fn = apply_contn;
	} else if (TYPE(fun) == FN || TYPE(fun) == SPECFORM) {
		/* Just call the function */
		(*ret)->data = NIL;
		(*ret)->fn = AS_FN(fun)->fn;
	} else /* lambda or macro */ {
		(*ret)->data = fun;
		(*ret)->fn = apply_closure;
	}
	return CAR(CDR(obj));
}

static struct obj *make_closure_validate(const char *name, enum objtype type, CPS_ARGS) {
	if (obj == NIL) {
		fprintf(stderr, "%s: must have args\n", name);
		*ret = &cfail;
		return NIL;
	}
	struct obj *args = CAR(obj);
	if (args != NIL && TYPE(args) != SYMBOL && TYPE(args) != CELL) {
		fprintf(stderr, "%s: expected symbol or list of symbols\n", name);
		*ret = &cfail;
		return NIL;
	}
	if (TYPE(args) == CELL) {
		for (; TYPE(args) == CELL; args = CDR(args)) {
			if (TYPE(CAR(args)) != SYMBOL) {
				fprintf(stderr, "%s: expected symbol or list of symbols\n", name);
				*ret = &cfail;
				return NIL;
			}
		}
		if (args != NIL && TYPE(args) != SYMBOL) {
			fprintf(stderr, "%s: expected symbol or list of symbols\n", name);
			*ret = &cfail;
			return NIL;
		}
	}
	if (TYPE(CDR(obj)) != CELL) {
		fprintf(stderr, "%s: invalid body\n", name);
		*ret = &cfail;
		return NIL;
	}
	*ret = self->next;
	struct obj *closure = make_closure(type, CAR(obj), CDR(obj), self->env);
	return closure;
}

static struct obj *fn_lambda(CPS_ARGS) {
	return make_closure_validate("lambda", LAMBDA, self, obj, ret);
}
static struct obj *fn_macro(CPS_ARGS) {
	return make_closure_validate("macro", MACRO, self, obj, ret);
}

static struct obj *fn_number_(CPS_ARGS) {
	if (!check_args("number?", obj, 1)) {
		*ret = &cfail;
		return NIL;
	}
	*ret = self->next;
	return TYPE(obj) == NUM ? TRUE : FALSE;
}

#define ARITH_OPS(arith) \
	arith(fn_plus, +) \
	arith(fn_minus, -) \
	arith(fn_times, *) \
	arith(fn_div, /, if(AS_NUM(CAR(obj))==0.){fputs("Warning: /: divide by zero\n", stderr);})
#define NONNUM(arg, op) \
	if (TYPE(arg) != NUM) { \
		fputs(#op ": argument not a number\n", stderr); \
		*ret = &cfail; \
		return NIL; \
	}
#define ARITH_FN(name, op, ...) \
static struct obj *name(CPS_ARGS) { \
	if (obj == NIL) { \
		fputs(#op ": no arguments given\n", stderr); \
		*ret = &cfail; \
		return NIL; \
	} \
	NONNUM(CAR(obj), op) \
	double val = AS_NUM(CAR(obj)); \
	for (obj = CDR(obj); TYPE(obj) == CELL; obj = CDR(obj)) { \
		NONNUM(CAR(obj), op) \
		__VA_ARGS__ \
		val = val op AS_NUM(CAR(obj)); \
	} \
	if (obj != NIL) { \
		NONNUM(obj, op) \
		__VA_ARGS__ \
		val = val op AS_NUM(obj); \
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
		return NIL;
	}
	if (TYPE(CAR(obj)) != NUM || TYPE(CAR(CDR(obj))) != NUM) {
		fputs("%: argument not a number\n", stderr);
		*ret = &cfail;
		return NIL;
	}
	*ret = self->next;
	return make_num(fmod(AS_NUM(CAR(obj)), AS_NUM(CAR(CDR(obj)))));
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
		return NIL; \
	} \
	if (TYPE(CAR(obj)) != NUM || TYPE(CAR(CDR(obj))) != NUM) { \
		fputs(#lispname ": argument not a number\n", stderr); \
		*ret = &cfail; \
		return NIL; \
	} \
	*ret = self->next; \
	return (AS_NUM(CAR(obj)) op AS_NUM(CAR(CDR(obj)))) ? TRUE : FALSE; \
}
COMPARE_OPS(COMPARE_FN)
#undef COMPARE_FN

static struct obj *fn_string_(CPS_ARGS) {
	if (!check_args("string?", obj, 1)) {
		*ret = &cfail;
		return NIL;
	}
	*ret = self->next;
	return TYPE(CAR(obj)) == STRING ? TRUE : FALSE;
}

static struct obj *fn_string_append(CPS_ARGS) {
	if (length(obj) < 0) {
		fputs("string-append: args must be a proper list\n", stderr);
		*ret = &cfail;
		return NIL;
	}
	struct obj *cur = obj;
	size_t cap = 0;
	for (; cur != NIL; cur = CDR(cur)) {
		if (TYPE(CAR(cur)) != STRING) {
			fputs("string-append: expected string, given ", stderr);
			print_on(stderr, CAR(cur), 1);
			fputc('\n', stderr);
			*ret = &cfail;
			return NIL;
		}
		cap += AS_STRING(CAR(cur))->len;
	}
	struct string *result = unsafe_make_uninitialized_str(cap);
	cur = obj;
	char *dest = result->str;
	for (cur = obj; cur != NIL; cur = CDR(cur)) {
		memcpy(dest, AS_STRING(CAR(cur))->str, AS_STRING(CAR(cur))->len);
		dest += AS_STRING(CAR(cur))->len;
	}
	*ret = self->next;
	return (struct obj *) result;
}

static struct obj *fn_string_compare(CPS_ARGS) {
	if (!check_args("string-compare", obj, 2)) {
		*ret = &cfail;
		return NIL;
	}
	if (TYPE(CAR(obj)) != STRING || TYPE(CAR(CDR(obj))) != STRING) {
		fputs("string-compare: expected string, given ", stderr);
		if (TYPE(CAR(obj)) != STRING) {
			print_on(stderr, CAR(obj), 1);
		} else {
			print_on(stderr, CAR(CDR(obj)), 1);
		}
		fputc('\n', stderr);
		*ret = &cfail;
		return NIL;
	}
	*ret = self->next;
	return make_num(stringcmp(AS_STRING(CAR(obj)), AS_STRING(CAR(CDR(obj)))));
}

static struct obj *fn_string_length(CPS_ARGS) {
	if (!check_args("string-length", obj, 1)) {
		*ret = &cfail;
		return NIL;
	}
	if (TYPE(CAR(obj)) != STRING) {
		fputs("string-length: expected string, given ", stderr);
		print_on(stderr, CAR(obj), 1);
		fputc('\n', stderr);
		*ret = &cfail;
		return NIL;
	}
	*ret = self->next;
	return make_num((double)(AS_STRING(CAR(obj))->len));
}

static struct obj *fn_substring(CPS_ARGS) {
	int nargs = length(obj);
	if (nargs < 0) {
		fputs("substring: args must be a proper list", stderr);
		*ret = &cfail;
		return NIL;
	}
	if (nargs < 2 || nargs > 3) {
		fprintf(stderr, "substring: expected 2 or 3 args, got %d\n", nargs);
		*ret = &cfail;
		return NIL;
	}
	if (TYPE(CAR(obj)) != STRING) {
		fputs("substring: expected string, given ", stderr);
		print_on(stderr, CAR(obj), 1);
		fputc('\n', stderr);
		*ret = &cfail;
		return NIL;
	}
	if (TYPE(CAR(CDR(obj))) != NUM || (nargs == 3 && TYPE(CAR(CDR(CDR(obj)))) != NUM)) {
		fputs("substring: expected number, given ", stderr);
		if (TYPE(CAR(CDR(obj))) != NUM) {
			print_on(stderr, CAR(CDR(obj)), 1);
		} else {
			print_on(stderr, CAR(CDR(CDR(obj))), 1);
		}
		fputc('\n', stderr);
		*ret = &cfail;
		return NIL;
	}
	size_t len = AS_STRING(CAR(obj))->len;

	double startd = AS_NUM(CAR(CDR(obj)));
	if (startd < 0) {
		fputs("substring: given negative start\n", stderr);
		*ret = &cfail;
		return NIL;
	}
	size_t start = (size_t)startd;
	if (startd != round(startd)) {
		fprintf(stderr, "Warning: substring: given non-integer %f, treating as %zu\n", startd, start);
	}
	if (start > len) {
		fprintf(stderr, "substring: start %zu greater than string length %zu\n", start, len);
		*ret = &cfail;
		return NIL;
	}

	size_t end = AS_STRING(CAR(obj))->len;
	if (nargs == 3) {
		double endd = AS_NUM(CAR(CDR(CDR(obj))));
		end = (size_t)endd;
		if (endd != round(endd)) {
			fprintf(stderr, "Warning: substring: given non-integer %f, treating as %zu\n", endd, end);
		}
		if (end < start) {
			fprintf(stderr, "substring: end %zu before start %zu\n", end, start);
			*ret = &cfail;
			return NIL;
		}
		if (end > len) {
			fprintf(stderr, "substring: end %zu greater than string length %zu\n", end, len);
			*ret = &cfail;
			return NIL;
		}
	}
	*ret = self->next;
	return (struct obj *) make_str_from_ptr_len(AS_STRING(CAR(obj))->str + start, end - start);
}

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
	DEFSYM(number?, fn_number_, FN);
	DEFSYM(pair?, fn_pair_, FN);
	DEFSYM(quote, fn_quote, SPECFORM);
	DEFSYM(set!, fn_set_, SPECFORM);
	DEFSYM(set-car!, fn_set_car_, FN);
	DEFSYM(set-cdr!, fn_set_cdr_, FN);
	DEFSYM(string?, fn_string_, FN);
	DEFSYM(string-append, fn_string_append, FN);
	DEFSYM(string-compare, fn_string_compare, FN);
	DEFSYM(string-length, fn_string_length, FN);
	DEFSYM(substring, fn_substring, FN);
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
