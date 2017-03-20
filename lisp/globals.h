#pragma once

struct env;
struct obj;

void add_globals(struct env *env);


/* Helpers for defining more builtins */

/* Length of list. -1 if atom or improper (dotted) list */
int length(struct obj *obj);
/* Makes sure `length(obj) == nargs` exactly, and
prints an error message if that's not the case. */
int check_args(const char *fn, struct obj *obj, int nargs);