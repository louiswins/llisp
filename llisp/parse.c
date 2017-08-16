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

static void skipws(struct input *i) {
	int ch;
	for (;;) {
		while ((ch = getch(i)) != EOF && isspace(ch));
		if (ch == ';') {
			/* skip comments */
			while ((ch = getch(i)) != EOF && ch != '\n');
		} else {
			break;
		}
	}
	ungetch(i, ch);
}

static struct string *read_token(struct input *i) {
	skipws(i);
	int ch = getch(i);
	if (ch == EOF) return NULL;
	struct string *s = make_str();
	s = str_append(s, (char)ch);
	if (isdelim(ch) || ch == '\'' || ch == '`' || ch == '"') {
		/* These tokens are complete by themselves */
		return s;
	} else if (ch == ',') {
		/* could be , or ,@ */
		ch = getch(i);
		if (ch == '@') {
			return str_append(s, (char)ch);
		} else {
			ungetch(i, ch);
			return s;
		}
	} else {
		while ((ch = getch(i)) != EOF && !isspace(ch) && !isdelim(ch)) {
			s = str_append(s, (char)ch);
		}
		ungetch(i, ch);
	}
	return s;
}

static struct obj *tryparsenum(struct string *s) {
	char *endp;
	double val = strtod(s->str, &endp);
	if (endp != s->str + s->len) {
		return NULL;
	}
	return make_num(val);
}

static struct obj *parse_one(struct input *i, struct string *tok);
static struct obj *parse_single(struct input *i);
static struct obj *parse_list(struct input *i) {
	struct obj *list = &nil;
	struct obj *cur = &nil;
	/* 0: no dot
	 * 1: read dot
	 * 2: read dot and one symbol after it */
	int isdot = 0;
	for (;;) {
		struct string *tok = read_token(i);
		if (!tok) {
			int ch = getch(i);
			if (ch == EOF) {
				fputs("Unexpected EOF", stderr);
			} else {
				ungetch(i, ch);
			}
			return NULL;
		}
		int ch = tok->str[0];
		if (isend(ch)) {
			if (isdot == 1) {
				fputs("Illegal dotted list: ignoring trailing dot", stderr);
			}
			ungetch(i, ch);
			return list;
		}
		if (tok->len == 1 && ch == '.') {
			if (isdot || list == &nil) {
				fprintf(stderr, "Illegal dotted list: %s\n", (isdot) ? "too many dots" : "no first element");
				return NULL;
			}
			isdot = 1;
			continue;
		}
		if (isdot == 2) {
			fputs("Illegal dotted list: too many elements after last dot", stderr);
			return NULL;
		}
		struct obj *obj = parse_one(i, tok);
		if (!obj) {
			return NULL;
		}
		if (isdot == 1) {
			cur->tail = obj;
			isdot = 2;
		} else if (list == &nil) {
			list = cur = cons(obj, &nil);
		} else {
			cur->tail = cons(obj, &nil);
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

static int ismatched(int start, int end) {
	return (start == '(' && end == ')')
		|| (start == '[' && end == ']')
		|| (start == '{' && end == '}');
}

static struct obj *parse_list_validate_end(char ch, struct input *i) {
	struct obj *ret = parse_list(i);
	if (!ret) return NULL;
	int end = getch(i);
	if (!ismatched(ch, end)) {
		fprintf(stderr, "Warning: unmatched delimiters: %c %c\n", ch, end);
	}
	return ret;
}

static struct obj *parse_one(struct input *i, struct string *tok) {
	char ch = tok->str[0];
	if (isbegin(ch)) {
		struct obj *ret = parse_list(i);
		int end = getch(i);
		if (!ismatched(ch, end)) {
			if (isend(end)) {
				fprintf(stderr, "Warning: unmatched delimiters: %c %c\n", ch, end);
			} else {
				return NULL;
			}
		}
		return ret;
	}
	if (isend(ch)) {
		fprintf(stderr, "Unmatched %c\n", ch);
		return NULL;
	}
	if (ch == '\'' || ch == '`' || ch == ',') {
		struct string *nexttok = read_token(i);
		if (!nexttok) return NULL;
		struct obj *next = parse_one(i, nexttok);
		if (!next) return NULL;
		struct string *sym;
		switch (ch) {
		default:
			fputs("Unknown quote syntax ", stderr);
			print_str_escaped(stderr, tok);
			return NULL;
		case '\'':
			sym = str_from_string_lit("quote");
			break;
		case '`':
			sym = str_from_string_lit("quasiquote");
			break;
		case ',':
			if (tok->len == 1) {
				sym = str_from_string_lit("unquote");
			} else {
				sym = str_from_string_lit("unquote-splicing");
			}
			break;
		}
		return cons(make_symbol(sym), cons(next, &nil));
	}
	if (ch == '"') {
		return parse_string(i);
	}
	if (ch == '+' || ch == '-' || ch == '.' || isdigit(ch)) {
		struct obj *ret = tryparsenum(tok);
		if (ret) {
			return ret;
		}
	}
	return make_symbol(tok);
}

static struct obj *parsenum(int sign) { (void)sign; return NULL; }

static struct obj *parse_single(struct input *i) {
	struct obj *tmp;
	int ch;
	for (;;) {
		ch = getch(i);
		if (ch == EOF) return NULL;
		if (isspace(ch)) continue;
		if (ch == ';') {
			while ((ch = getch(i)) != EOF && ch != '\n');
			continue;
		}
		break;
	}

	if (isbegin(ch)) {
		return parse_list_validate_end((char)ch, i);
	}
	if (isend(ch)) {
		fprintf(stderr, "Unmatched %c\n", ch);
		return NULL;
	}

	if (ch == '"') {
		return parse_string(i);
	}

#define PARSEQUOTE(symb) \
	do { \
		tmp = parse_single(i); \
		if (!tmp) return NULL; \
		return cons(make_symbol(str_from_string_lit(#symb)), cons(tmp, &nil)); \
	} while (0)
	if (ch == '\'') {
		PARSEQUOTE(quote);
	}
	if (ch == '`') {
		PARSEQUOTE(quasiquote);
	}
	if (ch == ',') {
		ch = peek(i);
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
		return parsenum(1);
	}
	if (ch == '-' && isdigit(peek(i))) {
		return parsenum(-1);
	}

	/* symbol */
	struct string *str = make_str();
	do {
		str = str_append(str, (char)ch);
	} while ((ch = getch(i)) != EOF && !isspace(ch) && !isdelim(ch));
	ungetch(i, ch);
	return make_symbol(str);
}

struct obj *parse(struct input *i) {
	gc_suspend();
	struct obj *ret = parse_single(i);
	gc_resume();
	return ret;
}
struct obj *parse_file(FILE *f) {
	struct input i = input_from_file(f);
	return parse(&i);
}