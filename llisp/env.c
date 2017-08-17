#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "env-private.h"
#include "gc.h"
#include "obj.h"

struct env *make_env(struct env *parent) {
	struct env *ret = gc_alloc(GC_ENV, sizeof(*ret));
	ret->parent = parent;
	init_hashtab(&ret->table);
	return ret;
}

void definesym(struct env *env, struct string *name, struct obj *value) {
	hashtab_put(&env->table, name, value);
}

int setsym(struct env *env, struct string *name, struct obj *value) {
	for (; env != NULL; env = env->parent) {
		if (hashtab_exists(&env->table, name)) {
			hashtab_put(&env->table, name, value);
			return 1;
		}
	}
	return 0;
}

struct obj *getsym(struct env *env, struct string *name) {
	for (; env != NULL; env = env->parent) {
		struct obj *o = hashtab_get(&env->table, name);
		if (o) return o;
	}
	return NULL;
}