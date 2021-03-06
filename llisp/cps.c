#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cps.h"
#include "gc.h"
#include "macroexpander.h"
#include "obj.h"
#include "print.h"

struct contn *dupcontn(struct contn *c) {
	struct contn *ret = (struct contn *) gc_alloc(CONTN, sizeof(*ret));
	memcpy(&ret->data, &c->data, sizeof(*ret) - offsetof(struct contn, data));
	return ret;
}

static int is_callable(enum objtype type) {
	return type == MACRO ||
		type == LAMBDA ||
		type == FN ||
		type == SPECFORM ||
		type == CONTN;
}

struct contn cend = { NULL };
struct contn cfail = { NULL };
struct contn cbegin = { NULL };

/* Eval an object */
struct obj *eval_cps(CPS_ARGS);
static struct obj *eval_doapply(CPS_ARGS);
static struct obj *eval_macroreeval(CPS_ARGS);
/* Try to eval directly instead of going through eval_cps
 * Only used if it can be done in constant time */
static _Bool direct_eval(struct obj *obj, struct env *env, struct obj **result);

/* Eval a list -> used for arguments to lambdas and cfuncs */
static struct obj *evallist(CPS_ARGS);
static struct obj *evallist_tailcons(CPS_ARGS);
static struct obj *evallist_cons(CPS_ARGS);

static struct obj *run_closure(CPS_ARGS);

_Bool direct_eval(struct obj *obj, struct env *env, struct obj **result) {
	if (!result) return 0;
	switch (TYPE(obj)) {
	default:
		return 0;
	case BUILTIN:
	case NUM:
	case SPECFORM:
	case FN:
	case LAMBDA:
	case MACRO:
	case STRING:
	case CONTN:
		*result = obj;
		return 1;
	case SYMBOL: {
		struct obj *value = getsym(env, AS_SYMBOL(obj));
		if (value == NULL) {
			return 0;
		}
		*result = value;
		return 1;
	}
	case CELL:
		return 0;
	}
}

/* obj = object to eval, return self->next(eval(obj)) */
struct obj *eval_cps(CPS_ARGS) {
	switch (TYPE(obj)) {
	default:
		fprintf(stderr, "eval: invalid type to eval %d\n", TYPE(obj));
		*ret = &cfail;
		return NIL;
	case BUILTIN:
	case NUM:
	case SPECFORM:
	case FN:
	case LAMBDA:
	case MACRO:
	case STRING:
	case CONTN:
		*ret = self->next;
		return obj;
	case SYMBOL: {
		struct obj *value = getsym(self->env, AS_SYMBOL(obj));
		if (value == NULL) {
			fputs("eval: unknown symbol \"", stderr);
			print_str_escaped(stderr, AS_SYMBOL(obj));
			fputs("\"\n", stderr);
			*ret = &cfail;
			return NIL;
		}
		*ret = self->next;
		return value;
	}
	case CELL: {
		if (CDR(obj) != NIL && TYPE(CDR(obj)) != CELL) {
			fputs("eval: function must be applied to proper list\n", stderr);
			*ret = &cfail;
			return NIL;
		}

		struct contn *doapply = dupcontn(self);
		doapply->data = obj;
		doapply->fn = eval_doapply;

		struct obj *result;
		if (direct_eval(CAR(obj), self->env, &result)) {
			*ret = doapply;
			return result;
		}

		/* direct evaluation failed - eval obj->head */
		*ret = dupcontn(self);
		(*ret)->next = doapply;
		return CAR(obj);
	}
	}
}

/* obj = fn, self->data = (fnname . args), call self->next(fn(obj))
 * We include fnname so that we can expand a macro if need be */
static struct obj *eval_doapply(CPS_ARGS) {
	if (!is_callable(TYPE(obj))) {
		fprintf(stderr, "apply: unable to apply non-function ");
		print_on(stderr, obj, 1 /*verbose*/);
		fputc('\n', stderr);
		*ret = &cfail;
		return NIL;
	}

	struct contn *appcnt = dupcontn(self);
	if (TYPE(obj) == CONTN) {
		appcnt->data = obj;
		appcnt->fn = apply_contn;
	} else if (TYPE(obj) == FN || TYPE(obj) == SPECFORM) {
		/* Just call the function */
		appcnt->data = NIL;
		appcnt->fn = AS_FN(obj)->fn;
	} else /* lambda or macro */ {
		appcnt->data = obj;
		appcnt->fn = apply_closure;
	}

	if (TYPE(obj) == FN || TYPE(obj) == LAMBDA || TYPE(obj) == CONTN) {
		/* need to evaluate the arguments first */
		*ret = dupcontn(self);
		(*ret)->data = NIL;
		(*ret)->next = appcnt;
		(*ret)->fn = evallist;
	} else /* specform, macro */ {
		/* ready to apply!*/
		*ret = appcnt;

		if (TYPE(obj) == MACRO) {
			fputs("apply: warning: applying a macro. This is now unexpected.\n", stderr);

			/* expand the macro in place and reeval afterwards */
			struct contn *expand = dupcontn(self);
			expand->fn = eval_macroreeval;
			appcnt->next = expand;
		}
	}

	return CDR(self->data);
}

/* obj = expansion, self->data = (macroname . args), call self->next(eval(obj)) */
static struct obj *eval_macroreeval(CPS_ARGS) {
	/* expand macro in place
	 * disabled due to design issues
	 * memcpy(self->data, obj, sizeof(*self->data)); */

	*ret = dupcontn(self);
	(*ret)->data = NIL;
	(*ret)->fn = eval_cps;
	return obj;
}


/* obj = (fn . list to eval) */
static struct obj *evallist(CPS_ARGS) {
	struct obj *result;
	if (direct_eval(obj, self->env, &result)) {
		*ret = self->next;
		return result;
	}
	if (TYPE(obj) != CELL) {
		*ret = dupcontn(self);
		(*ret)->fn = eval_cps;
		return obj;
	}
	/* call self->next(cons(eval(obj->head), evallist(obj->tail))) */
	struct contn *tailcons = dupcontn(self);
	tailcons->data = CDR(obj);
	tailcons->fn = evallist_tailcons;

	if (direct_eval(CAR(obj), self->env, &result)) {
		*ret = tailcons;
		return result;
	}

	*ret = dupcontn(self);
	(*ret)->next = tailcons;
	(*ret)->fn = eval_cps;
	return CAR(obj);
}
/* obj = eval(origobj->head), self->data = origobj->tail. call self->next(cons(obj, evallist(self->data))) */
static struct obj *evallist_tailcons(CPS_ARGS) {
	struct contn *docons = dupcontn(self);
	docons->data = obj;
	docons->fn = evallist_cons;

	*ret = dupcontn(self);
	(*ret)->data = NIL;
	(*ret)->next = docons;
	(*ret)->fn = evallist;
	return self->data;
}
/* obj = evallist(origobj->tail), self->data = eval(origobj->head). call self->next(cons(self->data, obj)) */
static struct obj *evallist_cons(CPS_ARGS) {
	*ret = self->next;
	return cons(self->data, obj);
}


/* obj = args, self->data = func, return self->next(func(args)) */
struct obj *apply_closure(CPS_ARGS) {
	/* set up environment */
	assert(TYPE(self->data) == LAMBDA || TYPE(self->data) == MACRO);
	struct closure *cdata = AS_CLOSURE(self->data);
	struct env *appenv = make_env(cdata->env);
	struct obj *params = cdata->args;
	for (;;) {
		if (params == NIL && obj == NIL) break;
		if (TYPE(params) == SYMBOL) {
			definesym(appenv, AS_SYMBOL(params), obj);
			params = NIL;
			break;
		}
		if (params == NIL) {
			fputs("warning: ", stderr);
			if (cdata->closurename) {
				print_str(stderr, cdata->closurename);
			} else {
				fputs("apply", stderr);
			}
			fputs(": too many arguments given\n", stderr);
			break;
		}
		if (obj == NIL) {
			if (cdata->closurename) {
				print_str(stderr, cdata->closurename);
			} else {
				fputs("apply", stderr);
			}
			fputs(": too few arguments given\n", stderr);
			*ret = &cfail;
			return NIL;
		}
		definesym(appenv, AS_SYMBOL(CAR(params)), CAR(obj));
		params = CDR(params);
		obj = CDR(obj);
	}

	*ret = dupcontn(self);
	(*ret)->data = cdata->code;
	(*ret)->env = appenv;
	(*ret)->fn = run_closure;
	return NIL;
}

/* obj = dontcare, self->data = code */
static struct obj *run_closure(CPS_ARGS) {
	(void)obj;

	*ret = dupcontn(self);
	(*ret)->data = NIL;
	(*ret)->fn = eval_cps;

	if (TYPE(CDR(self->data)) == CELL) {
		/* more code to run after this... */
		struct contn *finish = dupcontn(self);
		finish->data = CDR(self->data);
		(*ret)->next = finish;
	}

	return CAR(self->data);
}

/* obj = args, self->data = contnp, return contnp(args) */
struct obj *apply_contn(CPS_ARGS) {
	if (CDR(obj) != NIL) {
		fputs("warning: apply: too many arguments given\n", stderr);
	}
	*ret = AS_CONTN(self->data);
	return CAR(obj);
}

struct obj *run_cps(struct obj *obj, struct env *env) {
	// First macroexpand this puppy
	gc_current_obj = macroexpand_cps(obj, env);
	if (!gc_current_obj) {
		return NULL;
	}
	// Now run it for real
	cbegin.env = env;
	cbegin.next = &cend;
	cbegin.fn = eval_cps;
	gc_current_contn = &cbegin;
	struct contn *next = NULL;
	while (gc_current_contn != &cend && gc_current_contn != &cfail) {
		gc_current_obj = gc_current_contn->fn(gc_current_contn, gc_current_obj, &next);
		gc_current_contn = next;
		gc_cycle();
	}
	if (gc_current_contn == &cfail) {
		/* If we got `(obj)`, just return `obj`. */
		if (TYPE(gc_current_obj) == CELL && CDR(gc_current_obj) == NIL) {
			gc_current_obj = CAR(gc_current_obj);
		}
	}
	gc_current_contn = NULL;
	return gc_current_obj;
}
