#pragma once
#include "env.h"
#include "gc.h"

struct obj;

/*
 * A continuation expects to be given a simple louisp value. This is the obj pointer.
 * Instead of returning, the continuation invokes another continuation, giving it
 * another obj pointer. This is done by filling in the `ret' out param with the
 * continuation to invoke, and returning the argument.
 *
 * A continuation may have some associated data and an environment. It knows what to
 * do next and how to fail in the proper way.
 */

#define CPS_ARGS struct contn *self, struct obj *obj, struct contn **ret

struct contn {
	//struct gc_head data_;
	struct obj *data;
	struct env *env;
	struct contn *next;
	struct contn *fail;
	struct obj *(*fn)(CPS_ARGS);
};
/* duplicate an existing continuation */
struct contn *dupcontn(struct contn *c);

/* Evaluate obj in a continuation-passing style
 * Suitable for calling from specforms */
struct obj *eval_cps(CPS_ARGS);

/* Evaluate obj in env in a continuation-passing style
 * Suitable for calling at the top-level outside of any other
 * louisp computation */
struct obj *run_cps(struct obj *obj, struct env *env);

extern struct contn cend;
extern struct contn cfail;
extern struct contn cbegin;