#pragma once
#include "env.h"
#include "hashtab.h"

struct env {
	struct env *parent;
	struct hashtab table;
};