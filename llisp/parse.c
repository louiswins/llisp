#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "obj.h"
#include "parse.h"

static int file_ds_getc(struct data_source *ds) {
	return getc((FILE *)ds->rawdata);
}
static int file_ds_ungetc(int ch, struct data_source *ds) {
	return ungetc(ch, (FILE *)ds->rawdata);
}

void data_source_from_file(FILE *f, struct data_source *ds) {
	if (f == NULL || ds == NULL) {
		return;
	}
	ds->dsgetc = file_ds_getc;
	ds->dsungetc = file_ds_ungetc;
	ds->rawdata = f;
	ds->cur = NULL;
	ds->end = NULL;
}

static int mem_ds_getc(struct data_source *ds) {
	if (ds->cur == ds->end) {
		return EOF;
	}
	char ret = *(char *)ds->cur;
	ds->cur = ((char *)ds->cur) + 1;
	return ret;
}

static int mem_ds_ungetc(int ch, struct data_source *ds) {
	if (ch == EOF || ds->cur == ds->rawdata) {
		return EOF;
	}
	char *prev = (char *)ds->cur - 1;
	if (*prev != ch) {
		return EOF;
	}
	ds->cur = prev;
	return ch;
}

void data_source_from_memory(const char *s, size_t len, struct data_source *ds) {
	if (s == NULL || ds == NULL) {
		return;
	}
	ds->dsgetc = mem_ds_getc;
	ds->dsungetc = mem_ds_ungetc;
	/* Cast away const; this is fine because we never write to it. The FILE* one can't be const. */
	char *ms = (char *)s;
	ds->rawdata = ms;
	ds->cur = ms;
	ds->end = ms + len;
}

int line;

static int readch(struct data_source *ds) {
	int ret = ds->dsgetc(ds);
	if (ret == '\n') ++line;
	return ret;
}
static int unreadch(int ch, struct data_source *ds) {
	if (ch == '\n') --line;
	return ds->dsungetc(ch, ds);
}

static int peekch(struct data_source *ds) {
	// don't use readch/unreadch because we're not updating positions anyway
	int ret = ds->dsgetc(ds);
	ds->dsungetc(ret, ds);
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

static uint32_t read_unicode_escape(struct data_source *ds, int len) {
	uint32_t ret = 0;
	while (len--) {
		int ch = readch(ds);
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

static void read_string(struct data_source *ds) {
	_Bool unicodeerror = 0;
	struct string_builder sb;
	init_string_builder(&sb);
	for (;;) {
		uint32_t codepoint;
		int ch = readch(ds);
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
		ch = readch(ds);
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
			codepoint = read_unicode_escape(ds, 4 + 4 * (ch == 'U'));
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

static void read_token(struct data_source *ds) {
	int ch;
	for (;;) {
		ch = readch(ds);
		if (ch == EOF) {
			curtok.type = TT_EOF;
			return;
		}
		if (isspace(ch)) continue;
		if (ch == ';') {
			while ((ch = readch(ds)) != EOF && ch != '\n');
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
			int next = peekch(ds);
			if (next == '@') {
				readch(ds);
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
			read_string(ds);
			return;
	}

	// identifier, number, #t/#f, or .
	int validident = 1;
	struct string_builder sb;
	init_string_builder(&sb);
	do {
		if (!isprint(ch)) {
			validident = 0;
		}
		string_builder_append(&sb, (char) ch);
	} while ((ch = readch(ds)) != EOF && !isdelimiter((char) ch));
	unreadch(ch, ds);
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
}

static enum parse_result parse_one(struct data_source *ds, struct obj **result);
static enum parse_result parse_list(struct data_source *ds, struct obj **result) {
	struct obj *list = NIL;
	struct obj *cur = NIL;
	enum dot_status {
		NO_DOT,
		SEEN_DOT,
		DOT_AND_SYMBOL,
	} dot_status = NO_DOT;
	for (;;) {
		read_token(ds);
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
		enum parse_result ret = parse_one(ds, &obj);
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

static enum parse_result parse_one(struct data_source *ds, struct obj **result) {
#define QUOTE_CASE(type, name) \
		case type: { \
			read_token(ds); \
			struct obj *quoted; \
			enum parse_result ret = parse_one(ds, &quoted); \
			if (ret == PARSE_OK) { \
				*result = cons(intern_symbol(str_from_string_lit(#name)), cons(quoted, NIL)); \
			} \
			return ret; \
		}

	switch (curtok.type) {
		case TT_LPAREN:
			return parse_list(ds, result);
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

enum parse_result parse(struct data_source *ds, struct obj **result) {
	if (ds == NULL || result == NULL) {
		return PARSE_INVALIDPARAM;
	}
	enum parse_result ret = PARSE_INVALID;
	read_token(ds);
	return parse_one(ds, result);
}

void init_parser() {
	line = 1;
	curtok.type = TT_ERROR;
}
