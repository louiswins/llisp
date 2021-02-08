#pragma once
#include "obj.h"

/* Expand macros in obj in env in a continuation-passing style
 * Suitable for calling at the top-level outside of any other
 * llisp computation */
struct obj *macroexpand_cps(struct obj *obj, struct env *env);
