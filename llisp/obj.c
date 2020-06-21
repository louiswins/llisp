#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gc.h"
#include "obj.h"
#include "cps.h"

/* static objects */
struct obj nil = { NO_GC_HEAD, BUILTIN, .builtin = "()" };
struct obj true_ = { NO_GC_HEAD, BUILTIN, .builtin = "#t" };
struct obj false_ = { NO_GC_HEAD, BUILTIN, .builtin = "#f" };
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
		ret->str = name;
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
	ret->str = val;
	return ret;
}

struct obj *cons(struct obj *l, struct obj *r) {
	struct obj *ret = make_obj(CELL);
	ret->head = l;
	ret->tail = r;
	return ret;
}

/* strings */
struct string *unsafe_make_uninitialized_str(size_t len) {
	struct string *s = gc_alloc(GC_STR, offsetof(struct string, data) + len);
	s->len = len;
	return s;
}
struct string *make_str_from_ptr_len(const char *c, size_t len) {
	struct string *ret = unsafe_make_uninitialized_str(len);
	memcpy(STRING_DATA(ret), c, len);
	return ret;
}
void print_str(FILE *f, struct string *s) {
	fwrite(STRING_DATA(s), 1, s->len, f);
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
static void print_str_escaped_impl(FILE *f, char *s, size_t len) {
	for (size_t i = 0; i < len; ++i) {
		unsigned char ch = s[i];
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
void print_str_escaped(FILE *f, struct string *s) {
	print_str_escaped_impl(f, STRING_DATA(s), s->len);
}
void print_string_builder_escaped(FILE *f, struct string_builder *sb) {
	print_str_escaped_impl(f, STRING_DATA(sb->data), sb->used);
}

int stringeq(struct string *a, struct string *b) {
	if (a == b) return 1;
	if (!a || !b) return 0;
	if (a->len != b->len) return 0;
	return memcmp(STRING_DATA(a), STRING_DATA(b), a->len) == 0;
}
int stringcmp(struct string *a, struct string *b) {
	if (a == b) return 0;
	if (!a) return -1;
	if (!b) return 1;
	size_t minsize = a->len < b->len ? a->len : b->len;
	int ret = memcmp(STRING_DATA(a), STRING_DATA(b), minsize);
	if (ret) return ret;
	return (a->len < b->len) ? -1 : (a->len > b->len) ? 1 : 0;
}

void init_string_builder(struct string_builder *sb) {
	sb->data = unsafe_make_uninitialized_str(32); /* 32 is a good initial size */
	sb->used = 0;
}
void string_builder_append(struct string_builder *sb, char ch) {
	size_t cap = sb->data->len;
	if (sb->used == cap) {
		struct string *newdata = unsafe_make_uninitialized_str(cap + cap / 2);
		memcpy(STRING_DATA(newdata), STRING_DATA(sb->data), sb->used);
		sb->data = newdata;
	}
	STRING_DATA(sb->data)[sb->used++] = ch;
}
struct string *finish_string_builder(struct string_builder *sb) {
	if (sb->used == sb->data->len) {
		return sb->data;
	}
	return make_str_from_ptr_len(STRING_DATA(sb->data), sb->used);
}
