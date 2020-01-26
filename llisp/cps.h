#pragma once

struct env;
struct obj;

/*
 * A continuation expects to be given a simple llisp value. This is the obj pointer.
 * Instead of returning, the continuation invokes another continuation, giving it
 * another obj pointer. This is done by filling in the `ret' out param with the
 * continuation to invoke, and returning the argument.
 *
 * A continuation may have some associated data and an environment. It knows what to
 * do next and how to fail in the proper way.
 */

#define CPS_ARGS struct contn *self, struct obj *obj, struct contn **ret

struct contn {
	struct obj *data;
	struct env *env;
	struct contn *next;
	struct obj *(*fn)(CPS_ARGS);
};
/* duplicate an existing continuation */
struct contn *dupcontn(struct contn *c);

/* Evaluate obj in a continuation-passing style
 * Suitable for calling from specforms */
struct obj *eval_cps(CPS_ARGS);
/* Apply a closure (macro or lambda) */
struct obj* apply_closure(CPS_ARGS);
/* Apply a continuation */
struct obj* apply_contn(CPS_ARGS);

/* Try to eval directly instead of going through eval_cps
 * Only used if it can be done in constant time */
int direct_eval(struct obj *obj, struct env *env, struct obj **result);

/* Evaluate obj in env in a continuation-passing style
 * Suitable for calling at the top-level outside of any other
 * llisp computation */
struct obj *run_cps(struct obj *obj, struct env *env);

extern struct contn cend;
extern struct contn cfail;
extern struct contn cbegin;