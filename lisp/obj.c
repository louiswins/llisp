#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gc.h"
#include "obj.h"
#include "cps.h"

/* static objects */
struct obj nil = { BUILTIN, .builtin = "()" };
struct obj true_ = { BUILTIN, .builtin = "#t" };
struct obj false_ = { BUILTIN, .builtin = "#f" };


struct obj *make_obj(enum objtype type) {
	struct obj *ret = gc_alloc(GC_OBJ, sizeof(*ret));
	SETTYPE(ret, type);
	return ret;
}
struct obj *make_symbol(const char *name) {
	return make_symbol_len(name, strlen(name));
}
struct obj *make_symbol_len(const char *name, size_t len) {
	if (len >= MAXSYM) {
		fprintf(stderr, "make_symbol: symbol name too long: ");
		for (size_t i = 0; i < len; ++i) {
			fputc(name[i], stderr);
		}
		fputc('\n', stderr);
		return &nil;
	}
	struct obj *ret = make_obj(SYMBOL);
	memcpy(ret->sym, name, len);
	ret->sym[len] = '\0';
	return ret;
}
struct obj *make_num(double val) {
	struct obj *ret = make_obj(NUM);
	ret->num = val;
	return ret;
}
struct obj *make_fn(enum objtype type, struct obj *(*fn)(CPS_ARGS)) {
	struct obj *ret = make_obj(type);
	ret->fn = fn;
	return ret;
}

struct obj *cons(struct obj *l, struct obj *r) {
	struct obj *ret = make_obj(CELL);
	ret->head = l;
	ret->tail = r;
	return ret;
}


/* strings */
void init_str_alloc(struct string *s) {
	s->str = malloc(MAXSYM);
	if (s->str == NULL) abort();
	s->cap = MAXSYM;
	s->len = 0;
}
void init_str_ref(struct string *s, const char *c) {
	init_str_ref_len(s, c, 0);
}
void init_str_ref_len(struct string *s, const char *c, size_t len) {
	memset(s, 0, sizeof(*s));
	s->str = (char *)c; /* safe because cap==0 means we won't modify *str */
	s->len = len;
}
void free_str(struct string *s) {
	if (s->str && s->cap) {
		free(s->str);
	}
	s->cap = 0;
	s->str = NULL;
}
void print_str(FILE *f, struct string *s) {
	if (!s->str) return;
	for (size_t i = 0; i < s->len; ++i) {
		putc(s->str[i], f);
	}
}
void str_append(struct string *s, char ch) {
	if (s->str && !s->cap) {
		assert(s->str[s->len] == ch);
		++s->len;
	} else if (s->str) {
		if (s->len == s->cap) {
			s->cap += s->cap / 2;
			char *newstr = realloc(s->str, s->cap);
			if (newstr == NULL) abort();
			s->str = newstr;
		}
		s->str[s->len++] = ch;
	}
}