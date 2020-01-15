#pragma once
#include "gc.h"

struct gc_head {
	struct gc_head *next;
	uintptr_t marknext;
};

#define ISMARKED(o) ((o)->marknext & 0x1u)
#define ADDMARK(o) ((o)->marknext |= 0x1u)
#define DELMARK(o) ((o)->marknext &= ~0x1ull)
#define NEXTTOMARK(o) ((struct gc_head*)((o)->marknext & ~0xfull))
#define SETNEXTTOMARK(o, val) ((o)->marknext = (((o)->marknext & 0xfu) | (uintptr_t)val))

#define SIZE_OF_HEAD ((sizeof(struct gc_head) + GC_ALLOC_ALIGN - 1) & ~(GC_ALLOC_ALIGN - 1))

#define GC_FROM_OBJ(o) ((struct gc_head*)((char*)(o) - SIZE_OF_HEAD))
#define OBJ_FROM_GC(gc) ((void*)((char*)(gc) + SIZE_OF_HEAD))
