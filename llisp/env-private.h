#pragma once
#include "env.h"
#include "gc.h"
#include "hashtab.h"

struct env {
	struct obj o;
	struct env *parent;
	struct hashtab table;
};
