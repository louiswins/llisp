#pragma once

struct obj;

void print(struct obj *obj);
void display(struct obj *obj);
void print_on(FILE *f, struct obj *obj, int verbose);