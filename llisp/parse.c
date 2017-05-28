#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gc.h"
#include "obj.h"
#include "parse.h"

#define UNGET_NONE -2


struct input input_from_file(FILE *f) {
	struct input ret = { IN_FILE };
	ret.f = f;
	ret.ungotten = UNGET_NONE;
	return ret;
}
struct input input_from_string(const char *s) {
	struct input ret = { IN_STRING };
	ret.str = s;
	ret.offset = 0;
	return ret;
}

static int getch(struct input *i) {
	if (i->type == IN_FILE) {
		if (i->ungotten != UNGET_NONE) {
			int ret = i->ungotten;
			i->ungotten = UNGET_NONE;
			return ret;
		}
		return getc(i->f);
	} else {
		char ch = i->str[i->offset];
		if (ch) {
			++i->offset;
			return ch;
		}
		return EOF;
	}
}
static void ungetch(struct input *i, int ch) {
	if (i->type == IN_FILE) {
		assert(i->ungotten == UNGET_NONE);
		i->ungotten = ch;
	} else {
		assert(i->offset > 0);
		if (ch == EOF) {
			assert(i->str[i->offset] == '\0');
		} else {
			assert(i->str[i->offset - 1] == ch);
			--i->offset;
		}
	}
}

static int isbegin(int ch) { return ch == '(' || ch == '[' || ch == '{'; }
static int isend(int ch) { return ch == ')' || ch == ']' || ch == '}'; }
static int isdelim(int ch) { return isbegin(ch) || isend(ch); }
static int issym(int ch) { return isalnum(ch) || strchr("!$%&*+-./:<=>?@^_~#", ch); }
static int isidentstart(int ch) { return isalpha(ch) || strchr("!$%&*/:<=>?@^_~#", ch); } /* '+, '-, '->(etc) taken care of other places */

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
	struct string *s;
	skipws(i);
	int ch = getch(i);
	if (ch == EOF) return NULL;
	if (i->type == IN_FILE) {
		s = make_str();
	} else {
		s = make_str_ref(i->str + (i->offset - 1));
	}
	str_append(s, (char)ch);
	if (ch == '"') {
		int isescape = 0;
		for (;;) {
			ch = getch(i);
			if (ch == EOF) {
				fputs("Unterminated string\n", stderr);
				return NULL;
			}
			if (!isprint(ch)) {
				fprintf(stderr, "Invalid byte \\x%02X in string\n", ch);
				return NULL;
			}
			str_append(s, (char)ch);
			switch(ch) {
			default:
				isescape = 0;
				break;
			case '\\':
				isescape = !isescape;
				break;
			case '"':
				if (isescape)
					isescape = 0;
				else
					return s;
				break;
			}
		}

	} else if (!isdelim(ch)) {
		while ((ch = getch(i)) != EOF && !isspace(ch) && !isdelim(ch)) {
			str_append(s, (char)ch);
		}
		ungetch(i, ch);
	}
	return s;
}

static struct obj *tryparsenum(struct string *s) {
	char *endp;
	double val = strtod(s->str, &endp);
	if (endp != s->str + s->len) {
		fprintf(stderr, "Invalid number: ");
		print_str_escaped(stderr, s);
		fputs("", stderr);
		return NULL;
	}
	return make_num(val);
}

static struct obj *parse_one(struct input *i, struct string *tok);
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

static int parseonehex(int ch) {
	if (!isxdigit(ch)) {
		if (isprint(ch)) {
			fprintf(stderr, "Invalid hex digit %c\n", ch);
		} else {
			fprintf(stderr, "Invalid hex digit \\x%02X\n", ch);
		}
		return -1;
	}
	if (isdigit(ch)) ch -= '0';
	else if (isupper(ch)) ch -= 'A';
	else ch -= 'a';
	return ch;
}
static struct obj *validate_string(struct string *tok) {
	if (!memchr(tok->str, '\\', tok->len)) {
		/* Fast path: no escapes. Copy everything except the quotes */
		return make_str_obj(make_str_from_ptr_len(tok->str + 1, tok->len - 2));
	}
	/* Slow path */
	struct string *ret = make_str();
	const char *cur = tok->str + 1;
	const char *end = tok->str + tok->len - 1;
	for (; cur != end; ++cur) {
		if (*cur != '\\') {
			str_append(ret, *cur);
			continue;
		}
		++cur;
		switch (*cur) {
		case '\\':
			str_append(ret, '\\');
			break;
		case 'r':
			str_append(ret, '\r');
			break;
		case 'n':
			str_append(ret, '\n');
			break;
		case 't':
			str_append(ret, '\t');
			break;
		case '"':
			str_append(ret, '"');
			break;
		case 'x': {
			/* We know the string ends in ", so if we get hex digits we're ok */
			int val1 = parseonehex(*++cur);
			if (val1 < 0) return NULL;
			int val2 = parseonehex(*++cur);
			if (val2 < 0) return NULL;
			str_append(ret, (char)(val1 * 16 + val2));
			break;
		}
		default:
			if (isprint(*cur)) {
				fprintf(stderr, "Invalid escape \\%c\n", *cur);
			} else {
				fprintf(stderr, "Invalid escape \\(\\x%02X)\n", *cur);
			}
			return NULL;
		}
	}
	return make_str_obj(ret);
}

static int ismatched(int start, int end) {
	return (start == '(' && end == ')')
		|| (start == '[' && end == ']')
		|| (start == '{' && end == '}');
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
	if (ch == '"') {
		return validate_string(tok);
	}
	if (ch == '+' || ch == '-' || ch == '.' || isdigit(ch)) {
		/* Special case for '+ and '- and '->(whatever) symbols */
		if ((tok->len == 1 && (ch == '+' || ch == '-')) || (tok->len > 1 && ch == '-' && tok->str[1] == '>')) {
			return make_symbol(tok);
		}
		return tryparsenum(tok);
	}
	if (isidentstart(ch)) {
		return make_symbol(tok);
	}
	fputs("Invalid token: ", stderr);
	print_str_escaped(stderr, tok);
	fputs("", stderr);
	return NULL;
}

struct obj *parse(struct input *i) {
	struct obj *ret = NULL;
	gc_suspend();
	struct string *s = read_token(i);
	if (s) {
		ret = parse_one(i, s);
	}
	gc_resume();
	return ret;
}
struct obj *parse_file(FILE *f) {
	struct input i = input_from_file(f);
	return parse(&i);
}
struct obj *parse_string(const char *s) {
	struct input i = input_from_string(s);
	return parse(&i);
}