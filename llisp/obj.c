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
struct hashtab interned_symbols = EMPTY_HASHTAB;

struct obj *make_obj(enum objtype type) {
	struct obj *ret = gc_alloc(GC_OBJ, sizeof(*ret));
	SETTYPE(ret, type);
	return ret;
}
struct obj *make_symbol(struct string *name) {
	struct obj *existing = hashtab_get(&interned_symbols, name);
	// TODO: remove check for nil after implementing hashtable deletion
	if (existing && existing != &nil) {
		return existing;
	} else {
		struct obj *ret = make_obj(SYMBOL);
		ret->str = stringdup(name);
		hashtab_put(&interned_symbols, name, ret);
		return ret;
	}
}
struct obj *make_num(double val) {
	struct obj *ret = make_obj(NUM);
	ret->num = val;
	return ret;
}
struct obj *make_fn(enum objtype type, struct obj *(*fn)(CPS_ARGS), const char *name) {
	struct obj *ret = make_obj(type);
	ret->fn = fn;
	ret->fnname = name;
	return ret;
}
struct obj *make_str_obj(struct string *val) {
	struct obj *ret = make_obj(STRING);
	ret->str = stringdup(val);
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
struct string *make_str_cap(size_t cap) {
	struct string *s = gc_alloc(GC_STR, sizeof(*s) + cap);
	s->str = ((char *)s) + sizeof(*s);
	s->len = 0;
	s->cap = cap;
	return s;
}
struct string *make_str_ref(const char *c) {
	return make_str_ref_len(c, 0);
}
struct string *make_str_ref_len(const char *c, size_t len) {
	struct string *s = gc_alloc(GC_STR, sizeof(*s));
	s->str = (char *)c; /* it's ok: we won't actually try to modify it because cap == 0 */
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
	fwrite(s->str, 1, s->len, f);
}
#define BACKSLASH(ch) ((unsigned char)((ch) | 0x80))
unsigned char print_chars[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, BACKSLASH('t'), BACKSLASH('n'), 0, 0, BACKSLASH('r'), 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	' ', '!', BACKSLASH('"'), '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', BACKSLASH('\\'), ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', 0
};
#undef BACKSLASH
void print_str_escaped(FILE *f, struct string *s) {
	for (size_t i = 0; i < s->len; ++i) {
		unsigned char ch = s->str[i];
		if (ch < 0x80 && print_chars[ch]) {
			/* printable, with a possible escape */
			if (print_chars[ch] & 0x80)
				putc('\\', f);
			putc(print_chars[ch] & 0x7f, f);
		} else {
			/* nonprintable */
			fprintf(f, "\\x%02X", ch);
		}
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

struct string *str_append_str(struct string *s, struct string *s2) {
	if (!s->cap || s->cap - s->len < s2->len) {
		size_t newcap = s->cap ? (s->cap + s->cap / 2) : (s->len + s->len / 2);
		if (newcap - s->len < s2->len) {
			// Still not big enough... we'll give it room to breathe though.
			newcap += s2->len;
		}
		struct string *newstr = make_str_cap(newcap);
		memcpy(newstr->str, s->str, s->len);
		newstr->len = s->len;
		s = newstr;
	}
	memcpy(s->str + s->len, s2->str, s2->len);
	s->len += s2->len;
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
	if (a == b) return 1;
	if (!a || !b) return 0;
	if (a->len != b->len) return 0;
	return memcmp(a->str, b->str, a->len) == 0;
}
int stringcmp(struct string *a, struct string *b) {
	if (a == b) return 0;
	if (!a) return -1;
	if (!b) return 1;
	size_t minsize = a->len < b->len ? a->len : b->len;
	int ret = memcmp(a->str, b->str, minsize);
	if (ret) return ret;
	return (a->len < b->len) ? -1 : (a->len > b->len) ? 1 : 0;
}
