#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cps.h"
#include "gc.h"
#include "obj.h"
#include "print.h"

//#define MEMCPYPTR(c) (&((c)->data))
//#define CONTNOFFSETOFDATA(c) (MEMCPYPTR(c) - (c))
//#define MEMCPYSIZE(c) (sizeof(*(c)) - CONTNOFFSETOFDATA(c))
//#define COPYCONTNDATA(to, from) memcpy(MEMCPYPTR(to), MEMCPYPTR(from), MEMCPYSIZE(to))
#define COPYCONTNDATA(to, from) memcpy(to, from, sizeof(*to));
struct contn *dupcontn(struct contn *c) {
	struct contn *ret = gc_alloc(sizeof(*ret));
	COPYCONTNDATA(ret, c);
	return ret;
}

static int is_callable(enum objtype type) {
	return type == MACRO ||
		type == LAMBDA ||
		type == FN ||
		type == SPECFORM;
}

struct contn cend = { NULL };
struct contn cfail = { NULL };
struct contn cbegin = { NULL };

/* Eval an object */
struct obj *eval_cps(CPS_ARGS);
static struct obj *eval_doapply(CPS_ARGS);
static struct obj *eval_macroreeval(CPS_ARGS);

/* Eval a list -> used for arguments to lambdas and cfuncs */
static struct obj *evallist(CPS_ARGS);
static struct obj *evallist_tailcons(CPS_ARGS);
static struct obj *evallist_cons(CPS_ARGS);

/* Apply a closure (macro or lambda) */
static struct obj *apply_closure(CPS_ARGS);
static struct obj *run_closure(CPS_ARGS);


/* obj = object to eval, return self->next(eval(obj)) */
struct obj *eval_cps(CPS_ARGS) {
	switch (TYPE(obj)) {
	default:
		fprintf(stderr, "eval: unknown type %d\n", TYPE(obj));
		*ret = self->fail;
		return &nil;
	case BUILTIN:
	case NUM:
	case SPECFORM:
	case FN:
	case LAMBDA:
	case MACRO:
		*ret = self->next;
		return obj;
	case SYMBOL: {
		struct obj *value = getsym(self->env, obj->sym);
		if (value == NULL) {
			fprintf(stderr, "eval: unknown symbol \"%s\"\n", obj->sym);
			*ret = self->fail;
			return &nil;
		}
		*ret = self->next;
		return value;
	}
	case CELL: {
		if (obj->tail != &nil && TYPE(obj->tail) != CELL) {
			fputs("eval: function must be applied to proper list\n", stderr);
			*ret = self->fail;
			return &nil;
		}
		struct contn *doapply = dupcontn(self);
		doapply->data = obj;
		doapply->fn = eval_doapply;
		gc_add_to_temp_contns(doapply);

		/* eval obj->head */
		*ret = dupcontn(self);
		(*ret)->next = doapply;
		return obj->head;
	}
	}
}

/* obj = fn, self->data = (fnname . args), call self->next(fn(obj)) */
/* We include fnname so that we can expand a macro if need be */
static struct obj *eval_doapply(CPS_ARGS) {
	if (!is_callable(TYPE(obj))) {
		fprintf(stderr, "apply: unable to apply non-function ");
		print_debug(obj, NULL);
		fputc('\n', stderr);
		*ret = self->fail;
		return &nil;
	}

	struct contn *appcnt = dupcontn(self);
	if (TYPE(obj) == FN || TYPE(obj) == SPECFORM) {
		/* Just call the function */
		appcnt->data = &nil;
		appcnt->fn = obj->fn;
	} else /* lambda or macro */ {
		appcnt->data = obj;
		appcnt->fn = apply_closure;
	}
	gc_add_to_temp_contns(appcnt);

	if (TYPE(obj) == FN || TYPE(obj) == LAMBDA) {
		/* need to evaluate the arguments first */
		*ret = dupcontn(self);
		(*ret)->data = &nil;
		(*ret)->next = appcnt;
		(*ret)->fn = evallist;
	} else /* specform or macro */ {
		/* ready to apply!*/
		*ret = appcnt;

		if (TYPE(obj) == MACRO) {
			/* expand the macro in place and reeval afterwards */
			struct contn *expand = dupcontn(self);
			expand->fn = eval_macroreeval;
			appcnt->next = expand;
		}
	}

	return self->data->tail;
}

/* obj = expansion, self->data = (macroname . args), call self->next(eval(obj)) */
static struct obj *eval_macroreeval(CPS_ARGS) {
	/* expand macro in place */
	memcpy(self->data, obj, sizeof(*self->data));

	*ret = dupcontn(self);
	(*ret)->data = &nil;
	(*ret)->fn = eval_cps;
	return obj;
}


/* obj = (fn . list to eval) */
static struct obj *evallist(CPS_ARGS) {
	if (TYPE(obj) != CELL) {
		/* We're at the end of the list -> don't bother evaling nil */
		if (obj == &nil) {
			*ret = self->next;
			return &nil;
		} else {
			*ret = dupcontn(self);
			(*ret)->fn = eval_cps;
			return obj;
		}
	}
	/* call self->next(cons(eval(obj->head), evallist(obj->tail))) */
	struct contn *tailcons = dupcontn(self);
	tailcons->data = obj->tail;
	tailcons->fn = evallist_tailcons;
	gc_add_to_temp_contns(tailcons);

	*ret = dupcontn(self);
	(*ret)->next = tailcons;
	(*ret)->fn = eval_cps;
	return obj->head;
}
/* obj = eval(origobj->head), self->data = origobj->tail. call self->next(cons(obj, evallist(self->data))) */
static struct obj *evallist_tailcons(CPS_ARGS) {
	struct contn *docons = dupcontn(self);
	docons->data = obj;
	docons->fn = evallist_cons;
	gc_add_to_temp_contns(docons);

	*ret = dupcontn(self);
	(*ret)->data = &nil;
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
static struct obj *apply_closure(CPS_ARGS) {
	/* set up environment */
	struct env *appenv = make_env(self->data->env);
	gc_add_to_temp_envs(appenv);
	struct obj *params = self->data->args;
	for (;;) {
		if (params == &nil && obj == &nil) break;
		if (TYPE(params) == SYMBOL) {
			setsym(appenv, params->sym, obj);
			params = &nil;
			break;
		}
		if (params == &nil) {
			fputs("warning: apply: too many arguments given\n", stderr);
			break;
		}
		if (obj == &nil) {
			fputs("apply: too few arguments given\n", stderr);
			*ret = self->fail;
			return &nil;
		}
		setsym(appenv, params->head->sym, obj->head);
		params = params->tail;
		obj = obj->tail;
	}

	*ret = dupcontn(self);
	(*ret)->data = self->data->code;
	(*ret)->env = appenv;
	(*ret)->fn = run_closure;
	return &nil;
}

/* obj = dontcare, self->data = code */
static struct obj *run_closure(CPS_ARGS) {
	(void)obj;

	*ret = dupcontn(self);
	(*ret)->data = &nil;
	(*ret)->fn = eval_cps;

	if (TYPE(self->data->tail) == CELL) {
		/* more code to run after this... */
		gc_add_to_temp_contns(*ret);
		struct contn *finish = dupcontn(self);
		finish->data = self->data->tail;
		(*ret)->next = finish;
	}

	return self->data->head;
}


struct obj *run_cps(struct obj *obj, struct env *env) {
	cbegin.env = env;
	cbegin.next = &cend;
	cbegin.fail = &cfail;
	cbegin.fn = eval_cps;
	gc_current_contn = &cbegin;
	gc_current_obj = obj;
	struct contn *next = NULL;
	while (gc_current_contn != &cend && gc_current_contn != &cfail) {
		gc_clear_temp_roots();
		gc_current_obj = gc_current_contn->fn(gc_current_contn, gc_current_obj, &next);
		gc_current_contn = next;
	}
	if (gc_current_contn == &cfail) {
		puts("\nExecution failed.");
	}
	gc_current_contn = NULL;
	return gc_current_obj;
}