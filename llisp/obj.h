#pragma once
#include <stdio.h>
#include "env.h"
#include "hashtab.h"

#define CPS_ARGS struct contn *self, struct obj_union *obj, struct contn **ret

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

struct obj {
	struct obj *next;
	struct obj *marknext;
	enum objtype type;
	_Bool marked;
};

#define TYPE(o) (((struct obj*)(o))->type)
#define TYPEISOBJ(type) ((type) >= CELL && (type) <= OBJ_STRING)

#define STATIC_OBJ(type) { NULL, NULL, type, 0 }

struct string {
	struct obj o;
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
	struct obj o;
	struct obj_union *data;
	struct env *env;
	struct contn *next;
	struct obj_union *(*fn)(CPS_ARGS);
};

/* duplicate an existing continuation */
struct contn *dupcontn(struct contn *c);

struct closure {
	struct obj_union *args;
	struct obj_union *code;
	struct env *env;
	struct string *closurename;
};

struct obj_union {
	struct obj o;
	union {
		struct {
			struct obj_union *head;
			struct obj_union *tail;
		};
		double num;
		struct string *str;
		struct {
			struct obj_union *(*fn)(CPS_ARGS);
			const char *fnname;
		};
		struct closure closure;
		const char *builtin;
		struct contn *contnp;
	};
};
#define CAR(o) (((struct obj_union*)(o))->head)
#define CDR(o) (((struct obj_union*)(o))->tail)
#define AS_NUM(o) (((struct obj_union*)(o))->num)
#define AS_SYMBOL(o) ((struct obj_union*)(o))
#define AS_FN(o) ((struct obj_union*)(o))
#define AS_CLOSURE(o) (&((struct obj_union*)(o))->closure)
#define AS_BUILTIN(o) ((struct obj_union*)(o))
#define AS_OBJ_CONTN(o) (((struct obj_union*)(o))->contnp)
#define AS_OBJ_STR(o) (((struct obj_union*)(o))->str)
#define AS_CONTN(o) ((struct contn*)(o))
#define AS_STRING(o) ((struct string*)(o))

#define NIL (&nil)
#define TRUE (&true_)
#define FALSE (&false_)

extern struct obj_union nil;
extern struct obj_union true_;
extern struct obj_union false_;

// Weak references to every symbol
extern struct hashtab interned_symbols;

struct obj_union *make_obj(enum objtype type);
struct obj_union *make_symbol(struct string *name);
struct obj_union *make_num(double val);
struct obj_union *make_fn(enum objtype type, struct obj_union *(*fn)(CPS_ARGS), const char *name);
struct obj_union *make_str_obj(struct string *val);

struct obj_union *cons(struct obj_union *l, struct obj_union *r);
