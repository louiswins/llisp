#pragma once
#include "cps.h"
#include "env.h"
#include "gc.h"

struct string {
	char *str;
	size_t len;
	size_t cap; /* 0 if ref */
};
void init_str_alloc(struct string *s);
void init_str_ref(struct string *s, const char *c);
void init_str_ref_len(struct string *s, const char *c, size_t len);
void free_str(struct string *s);
void print_str(FILE *f, struct string *s);
void str_append(struct string *s, char ch);

enum objtype { CELL, NUM, SYMBOL, FN, SPECFORM, LAMBDA, MACRO, BUILTIN, CONTN, MAX };
struct obj {
	//struct gc_head data_;
	enum objtype typ;
	union {
		struct {
			struct obj *head;
			struct obj *tail;
		};
		double num;
		char sym[MAXSYM];
		struct obj *(*fn)(CPS_ARGS);
		struct {
			struct obj *args;
			struct obj *code;
			struct env *env;
		};
		char *builtin;
		struct contn *contnp;
	};
};
//#define TYPE(o) ((enum objtype)((o)->data_.data >> 2))
//#define SETTYPE(o, typ) ((o)->data_.data |= (unsigned char)(typ) << 2)
#define TYPE(o) ((o)->typ)
#define SETTYPE(o, typ) (TYPE(o) = (typ))

extern struct obj nil;
extern struct obj *pnil;
extern struct obj true_;
extern struct obj false_;

struct obj *make_obj(enum objtype type);
struct obj *make_symbol(const char *name);
struct obj *make_symbol_len(const char *name, size_t len);
struct obj *make_num(double val);
struct obj *make_fn(enum objtype type, struct obj *(*fn)(CPS_ARGS));

struct obj *cons(struct obj *l, struct obj *r);