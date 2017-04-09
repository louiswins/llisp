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
	gc_add_to_temp_roots(ret);
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
struct string *make_str() {
	return make_str_cap(MAXSYM);
}
struct string *make_str_cap(size_t len) {
	struct string *s = gc_alloc(GC_STR, sizeof(*s) + len);
	s->str = ((char *)s) + sizeof(*s);
	s->len = 0;
	s->cap = len;
	return s;
}
struct string *make_str_ref(const char *c) {
	return make_str_ref_len(c, 0);
}
struct string *make_str_ref_len(const char *c, size_t len) {
	struct string *s = gc_alloc(GC_STR, sizeof(*s));
	s->str = (char *)c; // it's ok: we won't actually try to modify it because cap == 0
	s->len = len;
	s->cap = 0;
	return s;
}
void print_str(FILE *f, struct string *s) {
	for (size_t i = 0; i < s->len; ++i) {
		putc(s->str[i], f);
	}
}
struct string *str_append(struct string *s, char ch) {
	if (!s->cap) {
		assert(s->str[s->len] == ch);
		++s->len;
	} else {
		if (s->len == s->cap) {
			struct string *newstr = make_str_cap(s->cap + s->cap / 2);
			memcpy(newstr->str, s->str, s->len);
			newstr->len = s->len;
			s = newstr;
		}
		s->str[s->len++] = ch;
	}
	return s;
}