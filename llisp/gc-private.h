#pragma once
#include "gc.h"

#define ISMARKED(o) (((struct obj *)(o))->marked)
#define ADDMARK(o) (((struct obj *)(o))->marked = 1)
#define DELMARK(o) (((struct obj *)(o))->marked = 0)
#define NEXTTOMARK(o) (((struct obj *)(o))->marknext)
#define SETNEXTTOMARK(o, val) (((struct obj *)(o))->marknext = (val))
