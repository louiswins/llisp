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
struct obj *make_symbol(struct string *name) {
	struct obj *ret = make_obj(SYMBOL);
	ret->sym = stringdup(name);
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
	return make_str_cap(32); /* 32 is a good initial size */
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
struct string *make_str_from_ptr_len(const char *c, size_t len) {
	struct string *ret = make_str_cap(len);
	memcpy(ret->str, c, len);
	ret->len = len;
	return ret;
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

struct string *stringdup(struct string *s) {
	if (s->cap) return s;
	struct string *ret = make_str_cap(s->len);
	memcpy(ret->str, s->str, s->len);
	ret->len = ret->cap = s->len;
	return ret;
}
int stringeq(struct string *a, struct string *b) {
	if (!a && !b) return 1;
	if (!a || !b) return 0;
	if (a->len != b->len) return 0;
	return memcmp(a->str, b->str, a->len) == 0;
}
int stringcmp(struct string *a, struct string *b) {
	if (!a && !b) return 0;
	if (!a) return -1;
	if (!b) return 1;
	size_t minsize = a->len < b->len ? a->len : b->len;
	int ret = memcmp(a->str, b->str, minsize);
	if (ret) return ret;
	return (a->len < b->len) ? -1 : (a->len > b->len) ? 1 : 0;
}