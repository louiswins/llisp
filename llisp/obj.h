#pragma once
#include <stdio.h>
#include "env.h"
#include "hashtab.h"

#define CPS_ARGS struct contn *self, struct obj *obj, struct contn **ret

enum objtype {
	CELL,

	NUM,
	FN,
	SPECFORM,
	BUILTIN,

	LAMBDA,
	MACRO,
	SYMBOL,
	STRING,
	CONTN,
	ENV,
	HASHTABARR
};

struct obj {
	struct obj *next;
	struct obj *marknext;
	enum objtype type;
	_Bool marked;
};

#define TYPE(o) ((o)->type)
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
_Bool stringeq(struct string *a, struct string *b); /* a == b */
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
 * A lisp pair (a . b).
 */
struct cell {
	struct obj o;
	struct obj *head;
	struct obj *tail;
};

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
	struct obj *data;
	struct env *env;
	struct contn *next;
	struct obj *(*fn)(CPS_ARGS);
};

/* duplicate an existing continuation */
struct contn *dupcontn(struct contn *c);

/*
 * A closure - a lambda or a macro. Keeps a reference to its argument names, the
 * code to run, and the environment upon creation (for the purposes of lexical
 * scope). Also remembers the first symbol that it is assigned to for help debugging.
 * For example, (define my_func (lambda () ...)) will associate 'my_func with
 * the lambda. Even (let ((my_func (lambda () ...))) ...) will do the same.
 */
struct closure {
	struct obj o;
	struct obj *args;
	struct obj *code;
	struct env *env;
	struct string *closurename;
};

struct obj_union {
	struct obj o;
	union {
		double num;
		struct {
			struct obj *(*fn)(CPS_ARGS);
			const char *fnname;
		};
		const char *builtin;
	};
};
#define CAR(o) (((struct cell*)(o))->head)
#define CDR(o) (((struct cell*)(o))->tail)
#define AS_NUM(o) (((struct obj_union*)(o))->num)
#define AS_FN(o) ((struct obj_union*)(o))
#define AS_CLOSURE(o) ((struct closure*)(o))
#define AS_BUILTIN(o) ((struct obj_union*)(o))
#define AS_CONTN(o) ((struct contn*)(o))
#define AS_SYMBOL(o) ((struct string*)(o))
#define AS_STRING(o) ((struct string*)(o))

#define NIL ((struct obj*)&nil)
#define TRUE ((struct obj*)&true_)
#define FALSE ((struct obj*)&false_)

extern struct obj_union nil;
extern struct obj_union true_;
extern struct obj_union false_;

/* Weak references to every symbol to support 'eq? */
extern struct hashtab interned_symbols;
struct obj *intern_symbol(struct string *name);

struct obj *make_obj(enum objtype type);
struct obj *make_num(double val);
struct obj *make_fn(enum objtype type, struct obj *(*fn)(CPS_ARGS), const char *name);
struct obj *make_closure(enum objtype type, struct obj *args, struct obj *code, struct env *env);

struct obj *cons(struct obj *l, struct obj *r);
