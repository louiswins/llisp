#pragma once
#include <stdio.h>

struct obj;

struct data_source {
	int (*dsgetc)(struct data_source *src);
	int (*dsungetc)(int ch, struct data_source *src);
	void *rawdata;
	void *cur;
	void *end;
};

void data_source_from_file(FILE *f, struct data_source *ds);
void data_source_from_memory(const char *s, size_t len, struct data_source *ds);

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

enum parse_result parse(struct data_source *ds, struct obj **result);
