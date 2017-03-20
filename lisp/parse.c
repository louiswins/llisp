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
static int isdelim(int ch) { return !!strchr("()[]{}", ch); }
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

static void read_token(struct input *i, struct string *s) {
	free_str(s);
	skipws(i);
	int ch = getch(i);
	if (ch == EOF) return;
	if (i->type == IN_FILE) {
		init_str_alloc(s);
	} else {
		init_str_ref(s, i->str + (i->offset - 1));
	}
	str_append(s, (char)ch);
	if (!isdelim(ch)) {
		while ((ch = getch(i)) != EOF && !isspace(ch) && !isdelim(ch)) {
			str_append(s, (char)ch);
		}
		ungetch(i, ch);
	}
}

static struct obj *tryparsenum(struct string *s) {
	char *endp;
	double val = strtod(s->str, &endp);
	if (endp != s->str + s->len) {
		fprintf(stderr, "Invalid number: ");
		print_str(stderr, s);
		fputs("", stderr);
		return NULL;
	}
	return make_num(val);
}

static struct obj *parse_one(struct input *i, struct string *tok);
static struct obj *parse_list(struct input *i) {
	struct obj *list = &nil;
	struct obj *cur = &nil;
	struct string tok = { NULL };
	/* 0: no dot
	 * 1: read dot
	 * 2: read dot and one symbol after it */
	int isdot = 0;
	for (;;) {
		read_token(i, &tok);
		if (!tok.str) {
			int ch = getch(i);
			if (ch == EOF) {
				fputs("Unexpected EOF", stderr);
			} else {
				ungetch(i, ch);
			}
			return NULL;
		}
		int ch = tok.str[0];
		if (isend(ch)) {
			if (isdot == 1) {
				fputs("Illegal dotted list: ignoring trailing dot", stderr);
			}
			free_str(&tok);
			ungetch(i, ch);
			return list;
		}
		if (tok.len == 1 && ch == '.') {
			if (isdot || list == &nil) {
				fprintf(stderr, "Illegal dotted list: %s\n", (isdot) ? "too many dots" : "no first element");
				free_str(&tok);
				return NULL;
			}
			isdot = 1;
			continue;
		}
		if (isdot == 2) {
			fputs("Illegal dotted list: too many elements after last dot", stderr);
			free_str(&tok);
			return NULL;
		}
		struct obj *obj = parse_one(i, &tok);
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
			fprintf(stderr, "Unmatched delimiters: %c %c\n", ch, end);
			if (!isend(end)) {
				return NULL;
			}
		}
		return ret;
	}
	if (isend(ch)) {
		fprintf(stderr, "Unmatched %c\n", ch);
		return NULL;
	}
	if (ch == '+' || ch == '-' || ch == '.' || isdigit(ch)) {
		if ((tok->len == 1 && (ch == '+' || ch == '-')) || (tok->len > 1 && ch == '-' && tok->str[1] == '>')) {
			if (tok->len >= MAXSYM) {
				fprintf(stderr, "Symbol too long: ");
				print_str(stderr, tok);
				fputs(". Truncating.", stderr);
				tok->len = MAXSYM - 1;
			}
			return make_symbol_len(tok->str, tok->len);
		}
		return tryparsenum(tok);
	}
	if (isidentstart(ch)) {
		if (tok->len >= MAXSYM) {
			fprintf(stderr, "Symbol too long: ");
			print_str(stderr, tok);
			fputs(". Truncating.", stderr);
			tok->len = MAXSYM - 1;
		}
		return make_symbol_len(tok->str, tok->len);
	}
	fprintf(stderr, "Invalid token: ");
	print_str(stderr, tok);
	fputs("", stderr);
	return NULL;
}

struct obj *parse(struct input *i) {
	gc_suspend();
	struct string s = { NULL };
	read_token(i, &s);
	if (!s.str) return NULL;
	struct obj *ret = parse_one(i, &s);
	free_str(&s);
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