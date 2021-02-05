#pragma once

struct obj_union;

/* Simple display of an object */
void display(struct obj_union *obj);
/* Verbose display of an object */
void print(struct obj_union *obj);

void print_on(FILE *f, struct obj_union *obj, int verbose);
