#pragma once
#include <stdio.h>

struct obj_union;

struct obj_union *parse(FILE *f);
void init_parser();
