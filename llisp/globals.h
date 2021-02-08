#pragma once

struct env;
struct obj;

void add_globals(struct env *env);
_Bool is_real_lambda(struct obj *obj, struct env *env);
_Bool is_real_define(struct obj *obj, struct env *env);
_Bool is_real_defmacro(struct obj *obj, struct env *env);

/* Helpers for defining more builtins */

/* Length of list. -1 if atom or improper (dotted) list */
int length(struct obj *obj);
/* Makes sure `length(obj) == nargs` exactly, and
prints an error message if that's not the case. */
_Bool check_args(const char *fn, struct obj *obj, int nargs);
