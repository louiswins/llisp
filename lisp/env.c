#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "env-private.h"
#include "gc.h"
#include "obj.h"

struct env *make_env(struct env *parent) {
	struct env *ret = gc_alloc(GC_ENV, sizeof(*ret));
	ret->parent = parent;
	ret->next = NULL;
	ret->nsyms = 0;
	return ret;
}

void setsym(struct env *env, struct string *name, struct obj *value) {
	for (;;) {
		for (int i = 0; i < env->nsyms; ++i) {
			if (stringeq(env->syms[i].name, name)) {
				env->syms[i].value = value ? value : &nil;
				return;
			}
		}
		if (env->next == NULL) break;
		env = env->next;
	}
	if (env->nsyms == ENVSIZE) {
		env->next = make_env(env);
		env = env->next;
		if (env == NULL) {
			fprintf(stderr, "setsym: out of memory\n");
			return;
		}
	}
	env->syms[env->nsyms].name = stringdup(name);
	env->syms[env->nsyms].value = value ? value : &nil;
	++env->nsyms;
}

struct obj *getsym(struct env *env, struct string *name) {
	for (; env != NULL; env = env->parent) {
		for (struct env *cur = env; cur != NULL; cur = cur->next) {
			for (int i = 0; i < cur->nsyms; ++i) {
				if (stringeq(cur->syms[i].name, name)) {
					return cur->syms[i].value;
				}
			}
		}
	}
	return NULL;
}

struct string *revlookup_debug(struct env *env, struct obj *val) {
	for (; env != NULL; env = env->parent) {
		for (struct env *cur = env; cur != NULL; cur = cur->next) {
			for (int i = 0; i < cur->nsyms; ++i) {
				if (cur->syms[i].value == val)
					return cur->syms[i].name;
			}
		}
	}
	return NULL;
}