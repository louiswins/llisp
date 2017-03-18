#pragma once
#include <stdint.h>

#define MAXSYM 32
struct env;
struct obj;

struct env *make_env(struct env *parent);

void setsym(struct env *env, const char *name, struct obj *value);
struct obj *getsym(struct env *env, const char *name);

/* debug stuff */
const char *revlookup_debug(struct env *env, struct obj *val);