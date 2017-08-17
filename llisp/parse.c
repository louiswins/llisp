#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gc.h"
#include "obj.h"
#include "parse.h"

#define UNGET_NONE -2
#define OBJ_NULL ((uintptr_t)0)
#define OBJ_DOT ((uintptr_t)0x1)
#define OBJ_CPAREN ((uintptr_t)0x4)
#define OBJ_CBRACKET ((uintptr_t)0x5)
#define OBJ_CBRACE ((uintptr_t)0x6)
#define OBJ_ISEND(o) ((o) & 0x4)
#define OBJ_TO_CHAR(o) ((char)(((273 - 19 * o) * o - 708) / 2))
#define OBJ_ISSPECIAL(o) ((o) < GC_ALLOC_ALIGN)

struct input input_from_file(FILE *f) {
	struct input ret = { NULL, UNGET_NONE };
	ret.f = f;
	return ret;
}

static int getch(struct input *i) {
	if (i->ungotten != UNGET_NONE) {
		int ret = i->ungotten;
		i->ungotten = UNGET_NONE;
		return ret;
	}
	return getc(i->f);
}
static void ungetch(struct input *i, int ch) {
	assert(i->ungotten == UNGET_NONE);
	i->ungotten = ch;
}
static int peek(struct input *i) {
	int ret = getch(i);
	ungetch(i, ret);
	return ret;
}

static int isbegin(int ch) { return ch == '(' || ch == '[' || ch == '{'; }
static int isend(int ch) { return ch == ')' || ch == ']' || ch == '}'; }
static int isdelim(int ch) { return isbegin(ch) || isend(ch); }

static uintptr_t parse_one(struct input *i);
static struct obj *parse_list(char begin, struct input *i) {
	struct obj *list = &nil;
	struct obj *cur = &nil;
	/* 0: no dot
	 * 1: read dot
	 * 2: read dot and one symbol after it */
	int isdot = 0;
	for (;;) {
		uintptr_t obj = parse_one(i);
		if (!obj) {
			return NULL;
		}
		if (obj == OBJ_DOT) {
			if (isdot || list == &nil) {
				fprintf(stderr, "Illegal dotted list: %s\n", (isdot) ? "too many dots" : "no first element");
				return NULL;
			}
			isdot = 1;
			continue;
		}
		if (OBJ_ISEND(obj)) {
			if (isdot == 1) {
				fputs("Illegal dotted list: ignoring trailing dot\n", stderr);
			}
			int ismatched =
				(begin == '(' && obj == OBJ_CPAREN) ||
				(begin == '[' && obj == OBJ_CBRACKET) ||
				(begin == '{' && obj == OBJ_CBRACE);
			if (!ismatched) {
				fprintf(stderr, "Warning: unmatched delimiters: %c %c\n", begin, OBJ_TO_CHAR(obj));
			}
			return list;
		}

		if (isdot == 2) {
			fputs("Illegal dotted list: too many elements after last dot\n", stderr);
			return NULL;
		}

		if (isdot == 1) {
			cur->tail = (struct obj *)obj;
			isdot = 2;
		} else if (list == &nil) {
			list = cur = cons((struct obj *)obj, &nil);
		} else {
			cur->tail = cons((struct obj *)obj, &nil);
			cur = cur->tail;
		}
	}
}

static uint32_t read_unicode_escape(struct input *i, int len) {
	uint32_t ret = 0;
	while (len--) {
		int ch = getch(i);
		if ('0' <= ch && ch <= '9') {
			ret = (ret << 4) | (ch - '0');
		} else if ('a' <= ch && ch <= 'f') {
			ret = (ret << 4) | (ch - 'a' + 10);
		} else if ('A' <= ch && ch <= 'F') {
			ret = (ret << 4) | (ch - 'A' + 10);
		} else {
			fprintf(stderr, "Invalid unicode escaped char '%c'\n", ch);
			return (uint32_t)-1; /* 0xFFFFFFFF is not a unicode character so it works as a sentinel */
		}
	}
	return ret;
}
static struct string *write_utf8(struct string *s, uint32_t codepoint) {
	if (codepoint <= 0x7f) {
		return str_append(s, (char)codepoint);
	}
	if (codepoint <= 0x7ff) {
		unsigned char ch = (unsigned char)(0xc0 | (codepoint >> 6));
		s = str_append(s, (char)ch);
		ch = 0x80 | (codepoint & 0x3f);
		return str_append(s, (char)ch);
	}
	if (codepoint <= 0xffff) {
		unsigned char ch = (unsigned char)(0xe0 | (codepoint >> 12));
		s = str_append(s, (char)ch);
		ch = 0x80 | ((codepoint >> 6) & 0x3f);
		s = str_append(s, (char)ch);
		ch = 0x80 | (codepoint & 0x3f);
		return str_append(s, (char)ch);
	}
	if (codepoint <= 0x10ffff) {
		unsigned char ch = (unsigned char)(0xf0 | (codepoint >> 18));
		s = str_append(s, (char)ch);
		ch = 0x80 | ((codepoint >> 12) & 0x3f);
		s = str_append(s, (char)ch);
		ch = 0x80 | ((codepoint >> 6) & 0x3f);
		s = str_append(s, (char)ch);
		ch = 0x80 | (codepoint & 0x3f);
		return str_append(s, (char)ch);
	}
	fprintf(stderr, "Invalid unicode codepoint %" PRIu32 "\n", codepoint);
	return s;
}

/* Precondition: should have already read the starting quote "
 * Postcondition: will consume the ending quote ", ready to parse again */
static struct obj *parse_string(struct input *i) {
	struct string *str = make_str();
	for (;;) {
		uint32_t codepoint;
		int ch = getch(i);
		if (ch == EOF) {
			fputs("Unclosed string\n", stderr);
			return NULL;
		}
		if (ch == '"') {
			return make_str_obj(str);
		}
		if (ch != '\\') {
			str = str_append(str, (char)ch);
			continue;
		}
		ch = getch(i);
		switch (ch) {
		default:
			fprintf(stderr, "Warning: unknown escape \\%c, treating as %c\n", ch, ch);
			/* fallthru */
		case '\\':
		case '\'':
		case '"':
			str = str_append(str, (char)ch);
			break;
		case 'r':
			str = str_append(str, '\r');
			break;
		case 'n':
			str = str_append(str, '\n');
			break;
		case 't':
			str = str_append(str, '\t');
			break;
		case 'u':
		case 'U':
			codepoint = read_unicode_escape(i, ch == 'u' ? 4 : 8);
			if (codepoint != (uint32_t)-1) {
				str = write_utf8(str, codepoint);
			}
			break;
		}
	}
}

static struct obj *parsenum(int sign, struct input *i) {
	struct string *str = make_str();
	int ch;
	char *endp;
	while (isdigit((ch = getch(i))) || ch == '.' || ch == 'e' || ch == 'E' || ch == '+' || ch == '-') {
		str = str_append(str, (char)ch);
	}
	ungetch(i, ch);

	double val = strtod(str->str, &endp);
	if (endp != str->str + str->len) {
		/* We know that we only have printable characters in str, so %s is safe */
		fprintf(stderr, "Invalid number %s%s\n", (sign > 0) ? "" : "-", str->str);
		return NULL;
	}
	return make_num(sign * val);
}

static uintptr_t parse_one(struct input *i) {
	int ch;
	for (;;) {
		ch = getch(i);
		if (ch == EOF) return OBJ_NULL;
		if (isspace(ch)) continue;
		if (ch == ';') {
			while ((ch = getch(i)) != EOF && ch != '\n');
			continue;
		}
		break;
	}

	if (isbegin(ch)) {
		return (uintptr_t)parse_list((char)ch, i);
	}
	if (ch == ')') return OBJ_CPAREN;
	if (ch == '}') return OBJ_CBRACE;
	if (ch == ']') return OBJ_CBRACKET;
	if (ch == '.') return OBJ_DOT;

	if (ch == '"') {
		return (uintptr_t)parse_string(i);
	}

#define PARSEQUOTE(symb) \
	do { \
		uintptr_t tmp = parse_one(i); \
		if (OBJ_ISSPECIAL(tmp)) return OBJ_NULL; \
		return (uintptr_t)cons(make_symbol(str_from_string_lit(#symb)), cons((struct obj *)tmp, &nil)); \
	} while (0)
	if (ch == '\'') {
		PARSEQUOTE(quote);
	}
	if (ch == '`') {
		PARSEQUOTE(quasiquote);
	}
	if (ch == ',') {
		ch = getch(i);
		if (ch == '@') {
			PARSEQUOTE(unquote-splicing);
		}
		ungetch(i, ch);
		PARSEQUOTE(unquote);
	}
#undef PARSEQUOTE

	/* number */
	if (isdigit(ch)) {
		ungetch(i, ch);
		return (uintptr_t)parsenum(1, i);
	}
	if (ch == '-' && isdigit(peek(i))) {
		return (uintptr_t)parsenum(-1, i);
	}

	/* symbol */
	struct string *str = make_str();
	do {
		str = str_append(str, (char)ch);
	} while ((ch = getch(i)) != EOF && !isspace(ch) && !isdelim(ch));
	ungetch(i, ch);
	return (uintptr_t)make_symbol(str);
}

struct obj *parse(struct input *i) {
	gc_suspend();
	uintptr_t ret = parse_one(i);
	gc_resume();
	if (OBJ_ISSPECIAL(ret))
		return NULL;
	return (struct obj *)ret;
}
struct obj *parse_file(FILE *f) {
	struct input i = input_from_file(f);
	return parse(&i);
}