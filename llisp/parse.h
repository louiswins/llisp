#pragma once
#include <stdio.h>

struct obj;

struct obj *parse(FILE *f);
void init_parser();
