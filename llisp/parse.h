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

struct obj *parse(struct data_source *ds);

void init_parser();
