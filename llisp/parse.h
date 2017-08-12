#pragma once
#include <stdio.h>

struct obj;

struct input {
	FILE *f;
	int ungotten;
};
struct input input_from_file(FILE *f);
struct obj *parse(struct input *i);

struct obj *parse_file(FILE *f);