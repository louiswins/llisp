#pragma once
#include "env.h"
#include "gc.h"
#include "hashtab.h"

struct env {
	struct gc_head gc;
	struct env *parent;
	struct hashtab table;
};
