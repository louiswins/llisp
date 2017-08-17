#pragma once
#include "hashtab.h"

struct ht_entry {
	struct string *key;
	struct obj *value;
};