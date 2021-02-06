#pragma once
#include <stdio.h>
#include "env.h"
#include "hashtab.h"

enum objtype {
	CELL,
	NUM,
	SYMBOL,
	STRING,
	BUILTIN,
	FN,
	SPECFORM,
	LAMBDA,
	MACRO,
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

struct contn;
#define CPS_ARGS struct contn *self, struct obj *obj, struct contn **ret


/*
 * A lisp pair (a . b).
 */
struct cell {
	struct obj o;
	struct obj *head;
	struct obj *tail;
};
#define CAR(o) (((struct cell*)(o))->head)
#define CDR(o) (((struct cell*)(o))->tail)

struct obj *cons(struct obj *l, struct obj *r);


/*
 * A number. Always a double, we don't support the numerical tower.
 */
struct num {
	struct obj o;
	double num;
};
#define AS_NUM(o) (((struct num*)(o))->num)

struct obj *make_num(double val);


/*
 * A builtin value. There are (currently) only three of them: (), #t, and #f.
 */
struct builtin {
	struct obj o;
	const char *name;
};
#define AS_BUILTIN(o) ((struct builtin*)(o))
#define STATIC_BUILTIN(name) { { NULL, NULL, BUILTIN, 0 }, name }


/*
 * A string. Immutable. Carries its own length. Is NOT nul-terminated.
 */
struct string {
	struct obj o;
	size_t len;
	char str[1];
};
#define AS_SYMBOL(o) ((struct string*)(o))
#define AS_STRING(o) ((struct string*)(o))

struct string *unsafe_make_uninitialized_str(size_t len);
struct string *make_str_from_ptr_len(const char *c, size_t len);
#define str_from_string_lit(lit) make_str_from_ptr_len(lit, sizeof(lit)-1)
void print_str(FILE *f, struct string *s);
void print_str_escaped(FILE *f, struct string *s);
_Bool stringeq(struct string *a, struct string *b); /* a == b */
int stringcmp(struct string *a, struct string *b); /* like strcmp(a, b) */

/*
 * A mutable string builder. Although it reuses the string type as its buffer
 * you must NOT use the "buf" variable as it does not maintain the invariants
 * of a normal string outside a string_builder. Call finish_string_builder
 * to turn the string_builder into a normal string suitable to be passed to
 * llisp functions, etc. Not intended to be allocated on the heap.
 */
struct string_builder {
	struct string *buf;
	size_t used;
};

void init_string_builder(struct string_builder *sb);
void string_builder_append(struct string_builder *sb, char ch);
/*
 * Turns string_builder sb into a full-fledged, immutable string. This
 * will empty out sb's internals - if you want to reuse it you must
 * call init_string_builder again (at which point it will be empty).
 */
struct string *finish_string_builder(struct string_builder *sb);
void print_string_builder_escaped(FILE *f, struct string_builder *sb);


/*
 * A function/special form implemented in C instead of lisp.
 */
struct fn {
	struct obj o;
	struct obj *(*fn)(CPS_ARGS);
	const char *fnname;
};
#define AS_FN(o) ((struct fn*)(o))

struct obj *make_fn(enum objtype type, struct obj *(*fn)(CPS_ARGS), const char *name);


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
#define AS_CLOSURE(o) ((struct closure*)(o))

struct obj *make_closure(enum objtype type, struct obj *args, struct obj *code, struct env *env);


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
#define AS_CONTN(o) ((struct contn*)(o))

/* duplicate an existing continuation */
struct contn *dupcontn(struct contn *c);


/* Builtins */
#define NIL ((struct obj*)&nil)
#define TRUE ((struct obj*)&true_)
#define FALSE ((struct obj*)&false_)

extern struct builtin nil;
extern struct builtin true_;
extern struct builtin false_;

/* Weak references to every symbol to support eq? */
extern struct hashtab interned_symbols;
struct obj *intern_symbol(struct string *name);
