#include "hashtab.h"
#include "obj.h"
#include "print.h"

void print_ht_entry(struct string *key, struct obj *entry, void *vfirst) {
	int *first = vfirst;
	if (*first) {
		*first = 0;
	} else {
		fputs(" | ", stdout);
	}
	printf("%.*s: ", (int)key->len, key->str);
	if (entry) {
		display(entry);
	} else {
		fputs("NULL", stdout);
	}
}

void print_all(struct hashtab *ht, char *title) {
	int first = 1;
	puts(title);
	hashtab_foreach(ht, print_ht_entry, &first);
	putchar('\n');
}

int main() {
	struct hashtab ht;
	gc_suspend();
	init_hashtab(&ht);

	hashtab_put(&ht, str_from_string_lit("one"), make_num(1.));
	print_all(&ht, "ONE");
	hashtab_put(&ht, str_from_string_lit("four"), make_num(4.));
	print_all(&ht, "FOUR");
	hashtab_put(&ht, str_from_string_lit("two"), make_num(2.));
	print_all(&ht, "TWO");
	hashtab_put(&ht, str_from_string_lit("five"), make_num(5.));
	print_all(&ht, "FIVE");
	hashtab_put(&ht, str_from_string_lit("three"), make_num(3.));
	print_all(&ht, "THREE");

	struct obj *five = hashtab_get(&ht, str_from_string_lit("five"));
	fputs("\n Five = ", stdout);
	if (five) {
		display(five);
		putchar('\n');
	} else {
		puts("NULL");
	}
	
	hashtab_put(&ht, str_from_string_lit("six"), make_num(6.));
	print_all(&ht, "SIX");
	hashtab_put(&ht, str_from_string_lit("three"), make_str_obj(str_from_string_lit("trois")));
	print_all(&ht, "THREE AGAIN");

	hashtab_put(&ht, str_from_string_lit("five"), make_str_obj(str_from_string_lit("cinq")));
	print_all(&ht, "FIVE AGAIN");
	hashtab_put(&ht, str_from_string_lit("two"), make_str_obj(str_from_string_lit("deux")));
	print_all(&ht, "THREE AGAIN");
	hashtab_put(&ht, str_from_string_lit("one"), make_str_obj(str_from_string_lit("un")));
	print_all(&ht, "ONE AGAIN");
	hashtab_put(&ht, str_from_string_lit("three"), make_str_obj(str_from_string_lit("tres")));
	print_all(&ht, "THREE AGAIN AGAIN");

	five = hashtab_get(&ht, str_from_string_lit("five"));
	fputs("\n Five = ", stdout);
	if (five) {
		display(five);
		putchar('\n');
	} else {
		puts("NULL");
	}

	printf("We have twelve? %s\n", hashtab_exists(&ht, str_from_string_lit("twelve")) ? "yes" : "no");
	struct obj *twelve = hashtab_get(&ht, str_from_string_lit("twelve"));
	fputs("\n Twelve = ", stdout);
	if (twelve) {
		display(twelve);
		putchar('\n');
	} else {
		puts("NULL");
	}

	hashtab_put(&ht, str_from_string_lit("null"), NULL);
	print_all(&ht, "\n\nWITH NULL");

	printf("We have null? %s\n", hashtab_exists(&ht, str_from_string_lit("null")) ? "yes" : "no");
	struct obj *null = hashtab_get(&ht, str_from_string_lit("twelve"));
	fputs("\n Null = ", stdout);
	if (null) {
		display(null);
		putchar('\n');
	} else {
		puts("NULL");
	}

	return 0;
}