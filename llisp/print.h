#pragma once

struct obj;

/* Simple display of an object */
void display(struct obj *obj);
/* Verbose display of an object */
void print(struct obj *obj);

void print_on(FILE *f, struct obj *obj, int verbose);
