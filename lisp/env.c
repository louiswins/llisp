#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "env-private.h"
#include "obj.h"

struct env *make_env(struct env *parent) {
	struct env *ret = gc_alloc(GC_ENV, sizeof(*ret));
	ret->parent = parent;
	ret->next = NULL;
	ret->nsyms = 0;
	return ret;
}

void setsym(struct env *env, const char *name, struct obj *value) {
	size_t namesize = strlen(name);
	if (namesize >= MAXSYM) {
		fprintf(stderr, "setsym: symbol name too long: \"%s\"\n", name);
		return;
	}
	for (;;) {
		for (int i = 0; i < env->nsyms; ++i) {
			if (strcmp(env->syms[i].name, name) == 0) {
				env->syms[i].value = value ? value : &nil;
				return;
			}
		}
		if (env->next == NULL) break;
		env = env->next;
	}
	if (env->nsyms == ENVSIZE) {
		gc_add_to_temp_roots(value);
		env->next = make_env(env);
		env = env->next;
		if (env == NULL) {
			fprintf(stderr, "setsym: out of memory\n");
			return;
		}
	}
	strcpy(env->syms[env->nsyms].name, name);
	env->syms[env->nsyms].value = value ? value : &nil;
	++env->nsyms;
}

struct obj *getsym(struct env *env, const char *name) {
	for (; env != NULL; env = env->parent) {
		for (struct env *cur = env; cur != NULL; cur = cur->next) {
			for (int i = 0; i < cur->nsyms; ++i) {
				if (strcmp(cur->syms[i].name, name) == 0) {
					return cur->syms[i].value;
				}
			}
		}
	}
	return NULL;
}

const char *revlookup_debug(struct env *env, struct obj *val) {
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