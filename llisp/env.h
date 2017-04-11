#pragma once
#include <stdint.h>

struct env;
struct obj;
struct string;

struct env *make_env(struct env *parent);

void setsym(struct env *env, struct string *name, struct obj *value);
struct obj *getsym(struct env *env, struct string *name);

/* debug stuff */
struct string *revlookup_debug(struct env *env, struct obj *val);