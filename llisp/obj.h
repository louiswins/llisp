#pragma once
#include "cps.h"
#include "env.h"

struct string {
	char *str;
	size_t len;
	size_t cap; /* 0 if ref */
};
struct string *make_str();
struct string *make_str_cap(size_t cap);
struct string *make_str_ref(const char *c); /* Defaults to length 0 */
struct string *make_str_ref_len(const char *c, size_t len);
struct string *make_str_from_ptr_len(const char *c, size_t len); /* Copies */
#define str_from_string_lit(lit) make_str_from_ptr_len(lit, sizeof(lit)-1)
void print_str(FILE *f, struct string *s);
struct string *str_append(struct string *s, char ch);
struct string *stringdup(struct string *s);
int stringeq(struct string *a, struct string *b); /* a == b */
int stringcmp(struct string *a, struct string *b); /* like strcmp(a, b) */

enum objtype { CELL, NUM, SYMBOL, FN, SPECFORM, LAMBDA, MACRO, BUILTIN, CONTN, MAX };
struct obj {
	enum objtype typ;
	union {
		struct {
			struct obj *head;
			struct obj *tail;
		};
		double num;
		struct string *sym;
		struct obj *(*fn)(CPS_ARGS);
		struct {
			struct obj *args;
			struct obj *code;
			struct env *env;
		};
		const char *builtin;
		struct contn *contnp;
	};
};
#define TYPE(o) ((o)->typ)
#define SETTYPE(o, typ) (TYPE(o) = (typ))

extern struct obj nil;
extern struct obj *pnil;
extern struct obj true_;
extern struct obj false_;

struct obj *make_obj(enum objtype type);
struct obj *make_symbol(struct string *name);
struct obj *make_num(double val);
struct obj *make_fn(enum objtype type, struct obj *(*fn)(CPS_ARGS));

struct obj *cons(struct obj *l, struct obj *r);