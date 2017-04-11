#pragma once

struct obj;

void print(struct obj *obj);

/* debug stuff */
#include "env.h"
void print_debug(struct obj *obj, struct env *env);