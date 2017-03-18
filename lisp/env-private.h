#pragma once
#include "env.h"
#include "gc.h"

struct symbol {
	char name[MAXSYM];
	struct obj *value;
};

#define ENVSIZE 32
struct env {
	struct gc_head data_;
	struct env *parent;
	struct env *next;
	unsigned char nsyms;
	struct symbol syms[ENVSIZE];
};