#pragma once
#include "gc.h"

#define ISMARKED(o) ((o)->marknext & 0x1u)
#define ADDMARK(o) ((o)->marknext |= 0x1u)
#define DELMARK(o) ((o)->marknext &= ~0x1ull)
#define NEXTTOMARK(o) ((struct gc_head*)((o)->marknext & ~0xfull))
#define SETNEXTTOMARK(o, val) ((o)->marknext = (((o)->marknext & 0xfu) | (uintptr_t)val))

#define GC_FROM_OBJ(o) ((struct gc_head*)(o))
#define OBJ_FROM_GC(gc) ((void*)(gc))
