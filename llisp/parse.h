#pragma once
#include <stdio.h>

struct obj;

struct input {
	enum inputtype { IN_FILE, IN_STRING } type;
	union {
		struct {
			FILE *f;
			int ungotten;
		};
		struct {
			const char *str;
			size_t offset;
		};
	};
};
struct input input_from_file(FILE *f);
struct input input_from_string(const char *s);

struct obj *parse(struct input *i);

struct obj *parse_file(FILE *f);
struct obj *parse_string(const char *s);