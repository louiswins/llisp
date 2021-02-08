#include "cps.h"
#include "env.h"
#include "gc.h"
#include "globals.h"
#include "macroexpander.h"

static struct obj *macroexpand_list(CPS_ARGS);
static struct obj *macroexpand_list_tailcons(CPS_ARGS);
static struct obj *macroexpand_list_cons(CPS_ARGS);
static struct obj *do_macroexpand(CPS_ARGS);

static struct obj *get_if_macro(struct obj *var, struct env *env) {
	if (TYPE(var) != SYMBOL) return NULL;
	struct obj *fn = getsym(env, AS_SYMBOL(var));
	if (!fn) return NULL;
	return (TYPE(fn) == MACRO) ? fn : NULL;
}

/* self->data = var-or-prototype */
static struct obj *defmacro_in_env(CPS_ARGS) {
	/* We don't actually care what the value IS during macro expansion; we just want to make sure it's not a macro */
	struct obj *sym = self->data;
	if (TYPE(sym) == CELL) sym = CAR(self->data);
	definesym(self->env, AS_SYMBOL(sym), NIL);

	/* return `(define ,var-or-prototype ,@macroexpanded-body) */
	*ret = self->next;
	return cons(intern_symbol(str_from_string_lit("define")), cons(self->data, obj));
}

static struct obj *macroexpand_rebuildlambda(CPS_ARGS) {
	*ret = self->next;
	return cons(intern_symbol(str_from_string_lit("lambda")), cons(self->data, obj));
}

/* Before calling this you must set up *ret to be a mutable contn with the correct next field. */
static struct obj *do_lambda(struct obj *args, struct obj *body, struct env *env, struct contn **ret) {
	struct env *newenv = make_env(env);
	struct obj *curarg = args;
	while (1) {
		if (TYPE(curarg) == CELL) {
			if (TYPE(CAR(curarg)) == SYMBOL) {
				definesym(newenv, AS_SYMBOL(CAR(curarg)), NIL);
			}
			curarg = CDR(curarg);
		} else {
			if (TYPE(curarg) == SYMBOL) {
				definesym(newenv, AS_SYMBOL(curarg), NIL);
			}
			break;
		}
	}

	/* body -> macroexpand_list -> self->next; */
	(*ret)->data = NIL;
	(*ret)->env = newenv;
	(*ret)->fn = macroexpand_list;
	return body;
}

static struct obj *do_macroexpand(CPS_ARGS) {
	if (TYPE(obj) != CELL) {
		*ret = self->next;
		return obj;
	}
	if (is_real_lambda(CAR(obj), self->env) && (TYPE(CDR(obj)) == CELL)) {
		struct contn *lambdacons = dupcontn(self);
		lambdacons->data = CAR(CDR(obj));
		lambdacons->fn = macroexpand_rebuildlambda;
		*ret = dupcontn(self);
		(*ret)->next = lambdacons;
		return do_lambda(CAR(CDR(obj)), CDR(CDR(obj)), self->env, ret);
	}
	struct contn *next = self->next;
	if (is_real_define(CAR(obj), self->env) && (TYPE(CDR(obj)) == CELL)) {
		struct obj *var = CAR(CDR(obj));

		next = dupcontn(self);
		next->data = var;
		next->fn = defmacro_in_env;

		if (TYPE(var) == CELL) {
			// do_lambda, then (given the body) call defmacro_in_env, then do next
			*ret = dupcontn(self);
			(*ret)->next = next;
			return do_lambda(CDR(var), CDR(CDR(obj)), self->env, ret);
		}

		// It's a normal, non-function define - just process the definition & then defmacro_in_env
		obj = CDR(CDR(obj));
	}
	struct obj *fn = get_if_macro(CAR(obj), self->env);
	if (fn) {
		struct contn *redo_macroexpand = dupcontn(self);
		redo_macroexpand->next = next;
		*ret = dupcontn(self);
		(*ret)->data = fn;
		(*ret)->fn = apply_closure;
		(*ret)->next = redo_macroexpand;
		return CDR(obj);
	}

	// not a macro application - just macroexpand everything.
	*ret = dupcontn(self);
	(*ret)->data = NIL;
	(*ret)->fn = macroexpand_list;
	(*ret)->next = next;
	return obj;
}

/* let macroexpand_list_tailcons(x) = x -> cons(x, macroexpand_list(CDR(obj))). */
/* Then call CAR(obj) -> do_macroexpand -> macroexpand_list_tailcons -> (self->next) */
static struct obj *macroexpand_list(CPS_ARGS) {
	if (TYPE(obj) != CELL) {
		*ret = self->next;
		return obj;
	}

	struct contn *tailcons = dupcontn(self);
	tailcons->data = CDR(obj);
	tailcons->fn = macroexpand_list_tailcons;

	*ret = dupcontn(self);
	(*ret)->next = tailcons;
	(*ret)->fn = do_macroexpand;
	return CAR(obj);
}

/* obj = do_macroexpand(origobj->head), self->data = origobj->tail. */
/* let macroexpand_list_cons(tail) = tail -> cons(obj, tail). */
/* Call (self->data) -> macroexpand_list -> macroexpand_list_cons -> (self->next) */
static struct obj *macroexpand_list_tailcons(CPS_ARGS) {
	struct contn *docons = dupcontn(self);
	docons->data = obj;
	docons->fn = macroexpand_list_cons;

	*ret = dupcontn(self);
	(*ret)->data = NIL;
	(*ret)->next = docons;
	(*ret)->fn = macroexpand_list;
	return self->data;
}
/* obj = macroexpand_list(origobj->tail), self->data = do_macroexpand(origobj->head). */
/* Call cons(self->data, obj) -> (self->next) */
static struct obj *macroexpand_list_cons(CPS_ARGS) {
	*ret = self->next;
	return cons(self->data, obj);
}

struct obj *macroexpand_cps(struct obj *obj, struct env *env) {
	cbegin.env = make_env(env); // don't mess with the actual environment passed in
	cbegin.next = &cend;
	cbegin.fn = do_macroexpand;
	gc_current_contn = &cbegin;
	gc_current_obj = obj;
	struct contn *next = NULL;
	while (gc_current_contn != &cend && gc_current_contn != &cfail) {
		gc_current_obj = gc_current_contn->fn(gc_current_contn, gc_current_obj, &next);
		gc_current_contn = next;
		gc_cycle();
	}
	if (gc_current_contn == &cfail) {
		return NULL;
	}
	gc_current_contn = NULL;
	return gc_current_obj;
}
