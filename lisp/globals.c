#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cps.h"
#include "env.h"
#include "globals.h"
#include "obj.h"
#include "print.h"

int length(struct obj *obj) {
	int ret = 0;
	for (; TYPE(obj) == CELL; obj = obj->tail) {
		++ret;
	}
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
		*ret = self->fail;
		return &nil;
	}
	if (len < 2 || len > 3) {
		fprintf(stderr, "if: expected 2 or 3 args, got %d\n", len);
		*ret = self->fail;
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
		*ret = self->fail;
		return &nil;
	}
	*ret = self->next;
	return obj->head;
}

static struct obj *fn_define(CPS_ARGS);
static struct obj *define_setsym(CPS_ARGS);

static struct obj *fn_define(CPS_ARGS) {
	assert(obj->typ >= 0 && obj->typ < MAX);
	if (!check_args("define", obj, 2)) {
		*ret = self->fail;
		return &nil;
	}
	if (TYPE(obj->head) != SYMBOL) {
		fputs("define: must define a symbol\n", stderr);
		*ret = self->fail;
		return &nil;
	}
	struct contn *resume = dupcontn(self);
	assert(obj->typ >= 0 && obj->typ < MAX);
	resume->data = obj->head;
	resume->fn = define_setsym;

	*ret = dupcontn(self);
	assert(obj->typ >= 0 && obj->typ < MAX);
	(*ret)->next = resume;
	(*ret)->fn = eval_cps;
	return obj->tail->head;
}

/* obj = eval(defn), self->data = sym */
static struct obj *define_setsym(CPS_ARGS) {
	setsym(self->env, self->data->sym, obj);
	*ret = self->next;
	return &nil;
}

static struct obj *fn_car(CPS_ARGS) {
	if (!check_args("car", obj, 1)) {
		*ret = self->fail;
		return &nil;
	}
	if (TYPE(obj->head) != CELL) {
		fputs("car: object not a pair\n", stderr);
		*ret = self->fail;
		return &nil;
	}
	*ret = self->next;
	return obj->head->head;
}
static struct obj *fn_cdr(CPS_ARGS) {
	if (!check_args("cdr", obj, 1)) {
		*ret = self->fail;
		return &nil;
	}
	if (TYPE(obj->head) != CELL) {
		fputs("cdr: object not a pair\n", stderr);
		*ret = self->fail;
		return &nil;
	}
	*ret = self->next;
	return obj->head->tail;
}
static struct obj *fn_cons(CPS_ARGS) {
	if (!check_args("cons", obj, 2)) {
		*ret = self->fail;
		return &nil;
	}
	*ret = self->next;
	return cons(obj->head, obj->tail->head);
}

static struct obj *fn_pair_(CPS_ARGS) {
	if (!check_args("pair?", obj, 1)) {
		*ret = self->fail;
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
		char symname[MAXSYM];
		(void)snprintf(symname, MAXSYM, " gensym%d", ++symnum); /* can't get a space outside gensym */
		*ret = self->next;
		return make_symbol(symname);
	} else {
		fputs("gensym: expected 0 or 1 args\n", stderr);
		*ret = self->fail;
		return &nil;
	}
}

static struct obj *fn_eq_(CPS_ARGS) {
	if (!check_args("eq?", obj, 2)) {
		*ret = self->fail;
		return &nil;
	}
	*ret = self->next;
	struct obj *a = obj->head;
	struct obj *b = obj->tail->head;
	if (TYPE(a) == NUM && TYPE(b) == NUM) {
		return a->num == b->num ? &true_ : &false_;
	}
	if (TYPE(a) == SYMBOL && TYPE(b) == SYMBOL) {
		return strcmp(a->sym, b->sym) == 0 ? &true_ : &false_;
	}
	return a == b ? &true_ : &false_;
}

static struct obj *fn_display(CPS_ARGS) {
	if (!check_args("display", obj, 1)) {
		*ret = self->fail;
		return &nil;
	}
	print(obj->head);
	*ret = self->next;
	return &nil;
}
static struct obj *fn_newline(CPS_ARGS) {
	if (!check_args("newline", obj, 0)) {
		*ret = self->fail;
		return &nil;
	}
	putchar('\n');
	*ret = self->next;
	return &nil;
}

static struct obj *fn_callcc(CPS_ARGS) {
	if (!check_args("call-with-current-continuation", obj, 0)) {
		*ret = self->fail;
		return &nil;
	}
	*ret = dupcontn(self);
	(*ret)->fn = eval_cps;
	struct obj *contp = make_obj(CONTN);
	contp->contnp = self->next;
	return contp;
}

static struct obj *fn_error(CPS_ARGS) {
	int len = length(obj);
	if (len >= 1) {
		fputs("error: custom messages not yet supported.\n", stderr);
	}
	*ret = self->fail;
	return &nil;
}


static struct obj *make_closure(const char *name, enum objtype type, CPS_ARGS) {
	if (obj == &nil) {
		fprintf(stderr, "%s: must have args\n", name);
		*ret = self->fail;
		return &nil;
	}
	struct obj *args = obj->head;
	if (args != &nil && TYPE(args) != SYMBOL && TYPE(args) != CELL) {
		fprintf(stderr, "%s: expected symbol or list of symbols\n", name);
		*ret = self->fail;
		return &nil;
	}
	if (TYPE(args) == CELL) {
		for (; TYPE(args) == CELL; args = args->tail) {
			if (TYPE(args->head) != SYMBOL) {
				fprintf(stderr, "%s: expected symbol or list of symbols\n", name);
				*ret = self->fail;
				return &nil;
			}
		}
		if (args != &nil && TYPE(args) != SYMBOL) {
			fprintf(stderr, "%s: expected symbol or list of symbols\n", name);
			*ret = self->fail;
			return &nil;
		}
	}
	if (TYPE(obj->tail) != CELL) {
		fprintf(stderr, "%s: invalid body\n", name);
		*ret = self->fail;
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
	return make_closure("lambda", LAMBDA, self, obj, ret);
}
static struct obj *fn_macro(CPS_ARGS) {
	return make_closure("macro", MACRO, self, obj, ret);
}

#define ARITH_OPS(arith) \
	arith(fn_plus, +) \
	arith(fn_minus, -) \
	arith(fn_times, *) \
	arith(fn_div, /, if(obj->head->num==0.){fputs("Warning: /: divide by zero\n", stderr);})
#define NAN(arg, op) \
	if (TYPE(arg) != NUM) { \
		fputs(#op ": argument not a number\n", stderr); \
		*ret = self->fail; \
		return &nil; \
	}
#define ARITH_FN(name, op, ...) \
static struct obj *name(CPS_ARGS) { \
	if (obj == &nil) { \
		fputs(#op ": no arguments given\n", stderr); \
		*ret = self->fail; \
		return &nil; \
	} \
	NAN(obj->head, op) \
	double val = obj->head->num; \
	for (obj = obj->tail; TYPE(obj) == CELL; obj = obj->tail) { \
		NAN(obj->head, op) \
		__VA_ARGS__ \
		val = val op obj->head->num; \
	} \
	if (obj != &nil) { \
		NAN(obj, op) \
		__VA_ARGS__ \
		val = val op obj->num; \
	} \
	struct obj *retobj = make_num(val); \
	*ret = self->next; \
	return retobj; \
}
ARITH_OPS(ARITH_FN)
#undef ARITH_FN
#undef NAN

#define COMPARE_OPS(compare) \
	compare(fn_cmpeq, "=", ==) \
	compare(fn_cmplt, "<", <) \
	compare(fn_cmpgt, ">", >) \
	compare(fn_cmple, "<=", <=) \
	compare(fn_cmpge, ">=", >=)
#define COMPARE_FN(cname, lispname, op) \
static struct obj *cname(CPS_ARGS) { \
	if (!check_args(lispname, obj, 2)) return &nil; \
	if (TYPE(obj->head) != NUM || TYPE(obj->tail->head) != NUM) { \
		fputs(lispname ": argument not a number\n", stderr); \
		*ret = self->fail; \
		return &nil; \
	} \
	*ret = self->next; \
	return (obj->head->num op obj->tail->head->num) ? &true_ : &false_; \
}
COMPARE_OPS(COMPARE_FN)
#undef COMPARE_FN

void add_globals(struct env *env) {
	setsym(env, "begin", make_fn(FN, fn_begin));
	setsym(env, "call-with-current-continuation", make_fn(FN, fn_callcc));
	setsym(env, "car", make_fn(FN, fn_car));
	setsym(env, "cdr", make_fn(FN, fn_cdr));
	setsym(env, "cons", make_fn(FN, fn_cons));
	setsym(env, "define", make_fn(SPECFORM, fn_define));
	setsym(env, "display", make_fn(FN, fn_display));
	setsym(env, "eq?", make_fn(FN, fn_eq_));
	setsym(env, "error", make_fn(FN, fn_error));
	setsym(env, "gensym", make_fn(FN, fn_gensym));
	setsym(env, "if", make_fn(SPECFORM, fn_if));
	setsym(env, "lambda", make_fn(SPECFORM, fn_lambda));
	setsym(env, "macro", make_fn(SPECFORM, fn_macro));
	setsym(env, "newline", make_fn(FN, fn_newline));
	setsym(env, "pair?", make_fn(FN, fn_pair_));
	setsym(env, "quote", make_fn(SPECFORM, fn_quote));
#define REGISTER_FN(name, op, ...) setsym(env, #op, make_fn(FN, name));
	ARITH_OPS(REGISTER_FN)
#undef REGISTER_FN
#define REGISTER_FN(cname, lispname, ...) setsym(env, lispname, make_fn(FN, cname));
	COMPARE_OPS(REGISTER_FN)
#undef REGISTER_FN
}