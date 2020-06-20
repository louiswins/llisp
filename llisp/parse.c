#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gc.h"
#include "obj.h"
#include "parse.h"

int line;

static int readch(FILE *f) {
	int ret = getc(f);
	if (ret == '\n') ++line;
	return ret;
}
static int unreadch(int ch, FILE *f) {
	if (ch == '\n') --line;
	return ungetc(ch, f);
}

static int peekch(FILE *f) {
	// don't use readch/unreadch because we're not updating positions anyway
	int ret = getc(f);
	ungetc(ret, f);
	return ret;
}

static int isdelimiter(char ch) {
	return isspace(ch) ||
		ch == '(' || ch == ')' || ch == '"' || ch == ',' ||
		ch == '\'' || ch == '`' || ch == ';';
}

enum token_type {
	TT_LPAREN,
	TT_RPAREN,
	TT_QUOTE,
	TT_QUASIQUOTE,
	TT_UNQUOTE,
	TT_UNQUOTE_SPL,
	TT_DOT,
	TT_IDENT,
	TT_STRING,
	TT_NUMBER,
	TT_EOF,
	TT_ERROR,
};

struct token {
	enum token_type type;
	union {
		struct string *str;
		double num;
	} as;
	int line;
};

struct token curtok;

static uint32_t read_unicode_escape(FILE *f, int len) {
	uint32_t ret = 0;
	while (len--) {
		int ch = readch(f);
		if ('0' <= ch && ch <= '9') {
			ret = (ret << 4) | (ch - '0');
		} else if ('a' <= ch && ch <= 'f') {
			ret = (ret << 4) | (ch - 'a' + 10);
		} else if ('A' <= ch && ch <= 'F') {
			ret = (ret << 4) | (ch - 'A' + 10);
		} else {
			return (uint32_t) -1; /* 0xFFFFFFFF is not a unicode character so it works as a sentinel */
		}
	}
	return ret;
}

static int write_utf8(struct string **s, uint32_t codepoint) {
	if (codepoint <= 0x7f) {
		*s = str_append(*s, (char)codepoint);
		return 1;
	}
	if (codepoint <= 0x7ff) {
		*s = str_append(*s, (char)(0xc0u | (codepoint >> 6)));
		*s = str_append(*s, (char)(0x80u | (codepoint & 0x3fu)));
		return 1;
	}
	if (codepoint <= 0xffff) {
		*s = str_append(*s, (char)(0xe0u | (codepoint >> 12)));
		*s = str_append(*s, (char)(0x80u | ((codepoint >> 6) & 0x3fu)));
		*s = str_append(*s, (char)(0x80u | (codepoint & 0x3fu)));
		return 1;
	}
	if (codepoint <= 0x10ffff) {
		*s = str_append(*s, (char)(0xf0u | (codepoint >> 18)));
		*s = str_append(*s, (char)(0x80u | ((codepoint >> 12) & 0x3fu)));
		*s = str_append(*s, (char)(0x80u | ((codepoint >> 6) & 0x3fu)));
		*s = str_append(*s, (char)(0x80u | (codepoint & 0x3fu)));
		return 1;
	}
	fprintf(stderr, "Invalid unicode codepoint %" PRIu32 "\n", codepoint);
	return 0;
}

static void read_string(FILE *f) {
	int haserror = 0;
	struct string *str = make_str();
	for (;;) {
		uint32_t codepoint;
		int ch = readch(f);
		if (ch == EOF) {
			fprintf(stderr, "[line %d]: unterminated string\n", curtok.line);
			curtok.type = TT_ERROR;
			return;
		}
		if (ch == '"') {
			if (haserror) {
				curtok.type = TT_ERROR;
			} else {
				curtok.type = TT_STRING;
				curtok.as.str = str;
			}
			return;
		}
		if (ch != '\\') {
			str = str_append(str, (char)ch);
			continue;
		}
		int escape_line = line;
		ch = readch(f);
		switch (ch) {
		default:
			fprintf(stderr, "[line %d]: unknown escape \\%c, treating as %c\n", escape_line, ch, ch);
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
			codepoint = read_unicode_escape(f, 4 + 4 * (ch == 'U'));
			if (codepoint != (uint32_t)-1) {
				if (!write_utf8(&str, codepoint)) {
					haserror = 1;
				}
			} else {
				fprintf(stderr, "[line %d]: invalid unicode escape sequence.\n", escape_line);
				haserror = 1;
			}
			break;
		}
	}
}

static int can_begin_num(char ch) {
	return isdigit(ch) || ch == '.' || ch == '+' || ch == '-';
}

static void read_token(FILE *f) {
	int ch;
	for (;;) {
		ch = readch(f);
		if (ch == EOF) {
			curtok.type = TT_EOF;
			return;
		}
		if (isspace(ch)) continue;
		if (ch == ';') {
			while ((ch = readch(f)) != EOF && ch != '\n');
			continue;
		}
		break;
	}

	curtok.line = line;
	switch (ch) {
		case '(':
			curtok.type = TT_LPAREN;
			return;
		case ')':
			curtok.type = TT_RPAREN;
			return;
		case '\'':
			curtok.type = TT_QUOTE;
			return;
		case ',': {
			int next = peekch(f);
			if (next == '@') {
				readch(f);
				curtok.type = TT_UNQUOTE_SPL;
			} else {
				curtok.type = TT_UNQUOTE;
			}
			return;
		}
		case '`':
			curtok.type = TT_QUASIQUOTE;
			return;
		case '"':
			read_string(f);
			return;
	}

	// identifier, number, or .
	int validident = 1;
	struct string* str = make_str();
	do {
		if (!isprint(ch)) {
			validident = 0;
		}
		str = str_append(str, (char) ch);
	} while ((ch = readch(f)) != EOF && !isdelimiter((char) ch));
	unreadch(ch, f);
	if (!validident) {
		fprintf(stderr, "[line %d]: invalid identifier ", curtok.line);
		print_str_escaped(stderr, str);
		fputc('\n', stderr);
		curtok.type = TT_ERROR;
		return;
	}
	if (str->len == 1 && str->str[0] == '.') {
		curtok.type = TT_DOT;
		return;
	}
	if (can_begin_num(str->str[0])) {
		char* endp;
		double val = strtod(str->str, &endp);
		if (endp == str->str + str->len) {
			curtok.type = TT_NUMBER;
			curtok.as.num = val;
			return;
		}
	}
	curtok.type = TT_IDENT;
	curtok.as.str = str;
}

static void print_curtok(FILE *f) {
	switch (curtok.type) {
	case TT_LPAREN:
		fputc('(', f);
		break;
	case TT_RPAREN:
		fputc(')', f);
		break;
	case TT_QUOTE:
		fputc('\'', f);
		break;
	case TT_QUASIQUOTE:
		fputc('`', f);
		break;
	case TT_UNQUOTE:
		fputc(',', f);
		break;
	case TT_UNQUOTE_SPL:
		fputs(",@", f);
		break;
	case TT_DOT:
		fputc('.', f);
		break;
	case TT_IDENT:
		print_str(f, curtok.as.str);
		break;
	case TT_STRING:
		fputc('"', f);
		print_str_escaped(f, curtok.as.str);
		fputc('"', f);
		break;
	case TT_NUMBER:
		fprintf(f, "%f", curtok.as.num);
		break;
	case TT_EOF:
		fputs("<EOF>", f);
		break;
	}
}

static void error(const char *message, ...) {
	fprintf(stderr, "[line %d]: Error", curtok.line);

	if (curtok.type == TT_EOF) {
		fprintf(stderr, " at end");
	} else if (curtok.type != TT_ERROR) {
		fprintf(stderr, " at ");
		print_curtok(stderr);
	}
	fputs(": ", stderr);

	va_list args;
	va_start(args, message);
	vfprintf(stderr, message, args);
	va_end(args);
}

static struct obj *parse_one(FILE *f);
static struct obj *parse_list(FILE *f) {
	struct obj *list = &nil;
	struct obj *cur = &nil;
	enum dot_status {
		NO_DOT,
		SEEN_DOT,
		DOT_AND_SYMBOL,
	} dot_status = NO_DOT;
	for (;;) {
		read_token(f);
		if (curtok.type == TT_DOT) {
			if (dot_status == SEEN_DOT) {
				error("too many dots");
				return NULL;
			} else if (list == &nil) {
				error("illegal dot - no first element");
				return NULL;
			}
			dot_status = SEEN_DOT;
			continue;
		}
		if (curtok.type == TT_RPAREN) {
			if (dot_status == SEEN_DOT) {
				error("illegal dot - no final element");
				return NULL;
			}
			return list;
		}
		if (dot_status == DOT_AND_SYMBOL) {
			error("too many elements after last dot");
			return NULL;
		}

		struct obj *obj = parse_one(f);
		if (!obj) {
			return NULL;
		}

		if (dot_status == SEEN_DOT) {
			cur->tail = obj;
			dot_status = DOT_AND_SYMBOL;
		} else if (list == &nil) {
			list = cur = cons(obj, &nil);
		} else {
			cur->tail = cons(obj, &nil);
			cur = cur->tail;
		}
	}
}

static struct obj *parse_one(FILE *f) {
#define QUOTE_CASE(type, name) \
		case type: { \
			read_token(f); \
			struct obj *quoted = parse_one(f); \
			if (!quoted) return NULL; \
			return cons(make_symbol(str_from_string_lit(#name)), cons(quoted, &nil)); \
		}

	switch (curtok.type) {
		case TT_LPAREN:
			return parse_list(f);
		QUOTE_CASE(TT_QUOTE, quote)
		QUOTE_CASE(TT_QUASIQUOTE, quasiquote)
		QUOTE_CASE(TT_UNQUOTE, unquote)
		QUOTE_CASE(TT_UNQUOTE_SPL, unquote-splicing)
		case TT_IDENT:
			return make_symbol(curtok.as.str);
		case TT_STRING:
			return make_str_obj(curtok.as.str);
		case TT_NUMBER:
			return make_num(curtok.as.num);
	}

	error("unexpected token");
	return NULL;

#undef QUOTE_CASE
}

struct obj *parse(FILE *f) {
	struct obj *ret = NULL;
	gc_suspend();
	read_token(f);
	if (curtok.type != TT_EOF) {
		ret = parse_one(f);
	}
	gc_resume();
	return ret;
}

void init_parser() {
	line = 1;
	curtok.type = TT_ERROR;
}
