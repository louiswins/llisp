#pragma once
#include "env.h"

struct symbol {
	struct string *name;
	struct obj *value;
};

#define ENVSIZE 32
struct env {
	struct env *parent;
	struct env *next;
	unsigned char nsyms;
	struct symbol syms[ENVSIZE];
};