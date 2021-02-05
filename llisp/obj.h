#pragma once
#include <stdio.h>
#include "env.h"
#include "hashtab.h"

#define CPS_ARGS struct contn *self, struct obj *obj, struct contn **ret

enum objtype {
	CELL,
	NUM,
	SYMBOL,
	FN,
	SPECFORM,
	LAMBDA,
	MACRO,
	BUILTIN,
	OBJ_CONTN,
	OBJ_STRING,

	BARE_CONTN,
	BARE_STR,
	ENV,
	HASHTABARR
};

struct gc_head {
	struct gc_head *next;
	struct gc_head *marknext;
	enum objtype type;
	_Bool marked;
};

#define TYPE(o) (((struct gc_head*)(o))->type)
#define TYPEISOBJ(type) ((type) >= CELL && (type) <= OBJ_STRING)

#define STATIC_OBJ(type) { NULL, NULL, type, 0 }

struct string {
	struct gc_head gc;
	size_t len;
	char str[1];
};

struct string *unsafe_make_uninitialized_str(size_t len);
struct string *make_str_from_ptr_len(const char *c, size_t len);
#define str_from_string_lit(lit) make_str_from_ptr_len(lit, sizeof(lit)-1)
void print_str(FILE *f, struct string *s);
void print_str_escaped(FILE *f, struct string *s);
int stringeq(struct string *a, struct string *b); /* a == b */
int stringcmp(struct string *a, struct string *b); /* like strcmp(a, b) */

struct string_builder {
	struct string *buf;
	size_t used;
};

void init_string_builder(struct string_builder *sb);
void string_builder_append(struct string_builder *sb, char ch);
struct string *finish_string_builder(struct string_builder *sb);
void print_string_builder_escaped(FILE *f, struct string_builder *sb);

/*
 * A continuation expects to be given a simple llisp value. This is the obj pointer.
 * Instead of returning, the continuation invokes another continuation, giving it
 * another obj pointer. This is done by filling in the `ret' out param with the
 * continuation to invoke, and returning the argument.
 *
 * A continuation may have some associated data and an environment. It knows what to
 * do next and how to fail in the proper way.
 */
struct contn {
	struct gc_head gc;
	struct obj *data;
	struct env *env;
	struct contn *next;
	struct obj *(*fn)(CPS_ARGS);
};

/* duplicate an existing continuation */
struct contn *dupcontn(struct contn *c);

struct closure {
	struct obj *args;
	struct obj *code;
	struct env *env;
	struct string *closurename;
};

struct obj {
	struct gc_head gc;
	union {
		struct {
			struct obj *head;
			struct obj *tail;
		};
		double num;
		struct string *str;
		struct {
			struct obj *(*fn)(CPS_ARGS);
			const char *fnname;
		};
		struct closure closure;
		const char *builtin;
		struct contn *contnp;
	};
};
#define CAR(o) (((struct obj*)(o))->head)
#define CDR(o) (((struct obj*)(o))->tail)
#define AS_NUM(o) (((struct obj*)(o))->num)
#define AS_SYMBOL(o) ((struct obj*)(o))
#define AS_FN(o) ((struct obj*)(o))
#define AS_CLOSURE(o) (&((struct obj*)(o))->closure)
#define AS_BUILTIN(o) ((struct obj*)(o))
#define AS_OBJ_CONTN(o) (((struct obj*)(o))->contnp)
#define AS_OBJ_STR(o) (((struct obj*)(o))->str)
#define AS_CONTN(o) ((struct contn*)(o))
#define AS_STRING(o) ((struct string*)(o))

#define NIL (&nil)
#define TRUE (&true_)
#define FALSE (&false_)

extern struct obj nil;
extern struct obj true_;
extern struct obj false_;

// Weak references to every symbol
extern struct hashtab interned_symbols;

struct obj *make_obj(enum objtype type);
struct obj *make_symbol(struct string *name);
struct obj *make_num(double val);
struct obj *make_fn(enum objtype type, struct obj *(*fn)(CPS_ARGS), const char *name);
struct obj *make_str_obj(struct string *val);

struct obj *cons(struct obj *l, struct obj *r);
