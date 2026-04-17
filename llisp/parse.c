#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "obj.h"
#include "parse.h"

static int buf_getc_impl(struct buf *buf, _Bool advance) {
	if (buf->cur == buf->end) {
		return EOF;
	}
	char ret = *(char *)buf->cur;
	buf->cur = ((char *)buf->cur) + advance;
	return ret;
}

void init_buf(const char *s, size_t len, struct buf *buf) {
	if (s == NULL || buf == NULL) {
		return;
	}
	buf->begin = s;
	buf->cur = s;
	buf->end = s + len;
}

int line;

static int readch(struct buf *buf) {
	int ret = buf_getc_impl(buf, 1);
	if (ret == '\n') ++line;
	return ret;
}

static int peekch(struct buf *buf) {
	return buf_getc_impl(buf, 0);
}

static int isdelimiter(char ch) {
	return isspace(ch) ||
		ch == '(' || ch == ')' || ch == '"' || ch == ',' ||
		ch == '\'' || ch == '`' || ch == ';' || ch == EOF;
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
	TT_SHARPT,
	TT_SHARPF,
	TT_EOF,
	TT_ERROR,
	TT_ERROR_UNTERMINATEDSTR,
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

static uint32_t read_unicode_escape(struct buf *buf, int len) {
	uint32_t ret = 0;
	while (len--) {
		int ch = readch(buf);
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

static int write_utf8(struct string_builder *sb, uint32_t codepoint) {
	if (codepoint <= 0x7f) {
		string_builder_append(sb, (char)codepoint);
		return 1;
	}
	if (codepoint <= 0x7ff) {
		string_builder_append(sb, (char)(0xc0u | (codepoint >> 6)));
		string_builder_append(sb, (char)(0x80u | (codepoint & 0x3fu)));
		return 1;
	}
	if (codepoint <= 0xffff) {
		string_builder_append(sb, (char)(0xe0u | (codepoint >> 12)));
		string_builder_append(sb, (char)(0x80u | ((codepoint >> 6) & 0x3fu)));
		string_builder_append(sb, (char)(0x80u | (codepoint & 0x3fu)));
		return 1;
	}
	if (codepoint <= 0x10ffff) {
		string_builder_append(sb, (char)(0xf0u | (codepoint >> 18)));
		string_builder_append(sb, (char)(0x80u | ((codepoint >> 12) & 0x3fu)));
		string_builder_append(sb, (char)(0x80u | ((codepoint >> 6) & 0x3fu)));
		string_builder_append(sb, (char)(0x80u | (codepoint & 0x3fu)));
		return 1;
	}
	fprintf(stderr, "Invalid unicode codepoint %" PRIu32 "\n", codepoint);
	return 0;
}

static void read_string(struct buf *buf) {
	_Bool unicodeerror = 0;
	struct string_builder sb;
	init_string_builder(&sb);
	for (;;) {
		uint32_t codepoint;
		int ch = readch(buf);
		if (ch == EOF) {
			fprintf(stderr, "[line %d]: unterminated string\n", curtok.line);
			curtok.type = TT_ERROR_UNTERMINATEDSTR;
			return;
		}
		if (ch == '"') {
			if (unicodeerror) {
				curtok.type = TT_ERROR;
			} else {
				curtok.type = TT_STRING;
				curtok.as.str = finish_string_builder(&sb);
			}
			return;
		}
		if (ch != '\\') {
			string_builder_append(&sb, (char)ch);
			continue;
		}
		int escape_line = line;
		ch = readch(buf);
		switch (ch) {
		default:
			fprintf(stderr, "[line %d]: unknown escape \\%c, treating as %c\n", escape_line, ch, ch);
			/* fallthru */
		case '\\':
		case '\'':
		case '"':
			string_builder_append(&sb, (char)ch);
			break;
		case 'r':
			string_builder_append(&sb, '\r');
			break;
		case 'n':
			string_builder_append(&sb, '\n');
			break;
		case 't':
			string_builder_append(&sb, '\t');
			break;
		case 'u':
		case 'U':
			codepoint = read_unicode_escape(buf, 4 + 4 * (ch == 'U'));
			if (codepoint != (uint32_t)-1) {
				if (!write_utf8(&sb, codepoint)) {
					unicodeerror = 1;
				}
			} else {
				fprintf(stderr, "[line %d]: invalid unicode escape sequence.\n", escape_line);
				unicodeerror = 1;
			}
			break;
		}
	}
}

static int can_begin_num(char ch) {
	return isdigit(ch) || ch == '.' || ch == '+' || ch == '-';
}

static void read_token(struct buf *buf) {
	int ch;
	for (;;) {
		ch = readch(buf);
		if (ch == EOF) {
			curtok.type = TT_EOF;
			return;
		}
		if (isspace(ch)) continue;
		if (ch == ';') {
			while ((ch = readch(buf)) != EOF && ch != '\n');
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
			int next = peekch(buf);
			if (next == '@') {
				readch(buf);
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
			read_string(buf);
			return;
	}

	// identifier, number, #t/#f, or .
	int validident = 1;
	struct string_builder sb;
	init_string_builder(&sb);
	for (;;) {
		if (!isprint(ch)) {
			validident = 0;
		}
		string_builder_append(&sb, (char) ch);
		ch = peekch(buf);
		if (isdelimiter((char)ch)) {
			break;
		}
		readch(buf);
	}
	if (!validident) {
		fprintf(stderr, "[line %d]: invalid identifier ", curtok.line);
		print_string_builder_escaped(stderr, &sb);
		fputc('\n', stderr);
		curtok.type = TT_ERROR;
		return;
	}
	if (sb.used == 1 && sb.buf->str[0] == '.') {
		curtok.type = TT_DOT;
		return;
	}
	if (can_begin_num(sb.buf->str[0])) {
		char* endp;
		double val = strtod(sb.buf->str, &endp);
		if (endp == sb.buf->str + sb.used) {
			curtok.type = TT_NUMBER;
			curtok.as.num = val;
			return;
		}
	}
	// Identifiers can't start with #
	if (sb.buf->str[0] == '#') {
		// But we do support #t and #f specially
		if (sb.used == 2) {
			if (sb.buf->str[1] == 't') {
				curtok.type = TT_SHARPT;
				return;
			} else if (sb.buf->str[1] == 'f') {
				curtok.type = TT_SHARPF;
				return;
			}
		}
		fprintf(stderr, "[line %d]: invalid identifier ", curtok.line);
		print_string_builder_escaped(stderr, &sb);
		fputc('\n', stderr);
		curtok.type = TT_ERROR;
		return;
	}
	curtok.type = TT_IDENT;
	curtok.as.str = finish_string_builder(&sb);
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
	case TT_SHARPT:
		fputs("#t", f);
		break;
	case TT_SHARPF:
		fputs("#f", f);
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
	fputs("\n", stderr);
}

static enum parse_result parse_one(struct buf *buf, struct obj **result);
static enum parse_result parse_list(struct buf *buf, struct obj **result) {
	struct obj *list = NIL;
	struct obj *cur = NIL;
	enum dot_status {
		NO_DOT,
		SEEN_DOT,
		DOT_AND_SYMBOL,
	} dot_status = NO_DOT;
	for (;;) {
		read_token(buf);
		if (curtok.type == TT_DOT) {
			if (dot_status == SEEN_DOT) {
				error("too many dots");
				return PARSE_INVALID;
			} else if (list == NIL) {
				error("illegal dot - no first element");
				return PARSE_INVALID;
			}
			dot_status = SEEN_DOT;
			continue;
		}
		if (curtok.type == TT_RPAREN) {
			if (dot_status == SEEN_DOT) {
				error("illegal dot - no final element");
				return PARSE_INVALID;
			}
			*result = list;
			return PARSE_OK;
		}
		if (dot_status == DOT_AND_SYMBOL) {
			error("too many elements after last dot");
			return PARSE_INVALID;
		}

		struct obj *obj;
		enum parse_result ret = parse_one(buf, &obj);
		if (ret == PARSE_EMPTY) {
			return PARSE_PARTIAL;
		} else if (ret != PARSE_OK) {
			return ret;
		}

		if (dot_status == SEEN_DOT) {
			CDR(cur) = obj;
			dot_status = DOT_AND_SYMBOL;
		} else if (list == NIL) {
			list = cur = cons(obj, NIL);
		} else {
			CDR(cur) = cons(obj, NIL);
			cur = CDR(cur);
		}
	}
}

static enum parse_result parse_one(struct buf *buf, struct obj **result) {
#define QUOTE_CASE(type, name) \
		case type: { \
			read_token(buf); \
			struct obj *quoted; \
			enum parse_result ret = parse_one(buf, &quoted); \
			if (ret == PARSE_OK) { \
				*result = cons(intern_symbol(str_from_string_lit(#name)), cons(quoted, NIL)); \
			} \
			return ret; \
		}

	switch (curtok.type) {
		case TT_LPAREN:
			return parse_list(buf, result);
		QUOTE_CASE(TT_QUOTE, quote)
		QUOTE_CASE(TT_QUASIQUOTE, quasiquote)
		QUOTE_CASE(TT_UNQUOTE, unquote)
		QUOTE_CASE(TT_UNQUOTE_SPL, unquote-splicing)
		case TT_IDENT:
			*result = intern_symbol(curtok.as.str);
			return PARSE_OK;
		case TT_STRING:
			*result = (struct obj *)curtok.as.str;
			return PARSE_OK;
		case TT_NUMBER:
			*result = make_num(curtok.as.num);
			return PARSE_OK;
		case TT_SHARPT:
			*result = TRUE;
			return PARSE_OK;
		case TT_SHARPF:
			*result = FALSE;
			return PARSE_OK;
		case TT_ERROR_UNTERMINATEDSTR:
			return PARSE_PARTIAL;
		case TT_EOF:
			return PARSE_EMPTY;
	}

	error("unexpected token");
	return PARSE_INVALID;

#undef QUOTE_CASE
}

enum parse_result parse(struct buf *buf, struct obj **result) {
	if (buf == NULL || result == NULL) {
		return PARSE_INVALIDPARAM;
	}
	line = 1;
	curtok.type = TT_ERROR;

	enum parse_result ret = PARSE_EMPTY;
	*result = NIL;
	struct obj **next = result;
	struct obj *cur;
	for (;;) {
		read_token(buf);
		ret = parse_one(buf, &cur);
		if (ret == PARSE_OK) {
			*next = cons(cur, NIL);
			next = &CDR(*next);
		} else {
			break;
		}
	}
	if (ret == PARSE_EMPTY && *result != NIL) {
		/* The call to parse_one after we have parsed the last form will
		 * return PARSE_EMPTY. But the overall parse succeeded. */
		return PARSE_OK;
	}
	return ret;
}
