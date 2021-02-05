#pragma once
#include "gc.h"

#define ISMARKED(o) ((o)->marked)
#define ADDMARK(o) ((o)->marked = 1)
#define DELMARK(o) ((o)->marked = 0)
#define NEXTTOMARK(o) ((o)->marknext)
#define SETNEXTTOMARK(o, val) ((o)->marknext = (val))

#define GC_FROM_OBJ(o) ((struct obj*)(o))
#define OBJ_FROM_GC(gc) ((void*)(gc))
