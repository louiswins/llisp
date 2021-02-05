#pragma once
#include "obj.h"

/* Evaluate obj in a continuation-passing style
 * Suitable for calling from specforms */
struct obj_union *eval_cps(CPS_ARGS);
/* Apply a closure (macro or lambda) */
struct obj_union* apply_closure(CPS_ARGS);
/* Apply a continuation */
struct obj_union* apply_contn(CPS_ARGS);

/* Evaluate obj in env in a continuation-passing style
 * Suitable for calling at the top-level outside of any other
 * llisp computation */
struct obj_union *run_cps(struct obj_union *obj, struct env *env);

extern struct contn cend;
extern struct contn cfail;
extern struct contn cbegin;
