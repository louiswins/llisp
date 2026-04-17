#pragma once

struct obj;
struct buf {
	const char *begin;
	const char *end;
	const char *cur;
};

void init_buf(const char *s, size_t len, struct buf *buf);

enum parse_result {
	/* Fully parsed an entire result */
	PARSE_OK,
	/* Invalid parameter - this is an interpreter bug */
	PARSE_INVALIDPARAM,
	/* Invalid code */
	PARSE_INVALID,
	/* Partial form (e.g. parsing "(quote a" without a close paren) */
	PARSE_PARTIAL,
	/* Reached EOF but did not find anything but whitespace/comments/etc. */
	PARSE_EMPTY,
};

enum parse_result parse(struct buf* buf, struct obj **result);
