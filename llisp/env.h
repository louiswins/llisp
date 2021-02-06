#pragma once
#include <stdint.h>

struct env;
struct obj;
struct string;

struct env *make_env(struct env *parent);

/* Sets name=value in the current environment, defining it if it doesn't yet exist.
 * Will not alter parent environments.
 * Acts like (define name value) */
void definesym(struct env *env, struct string *name, struct obj *value);
/* Sets the closest binding for name to value. Will not define a new symbol.
 * Returns 1 if value was set, 0 if there is no visible binding for name.
 * Acts like (set! name value) */
_Bool setsym(struct env *env, struct string *name, struct obj *value);
struct obj *getsym(struct env *env, struct string *name);
