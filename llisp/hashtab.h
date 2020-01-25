#pragma once
#include <stdlib.h>

struct ht_entry;
struct obj;
struct string;

/* Hash table 
 * Supports insertion and lookup; no deletion */
struct hashtab {
	/* Number of entries */
	size_t size;
	/* Capacity of table */
	size_t cap;
	/* Array of entries */
	struct ht_entry *table;
};

/* Initialize an empty hashtable */
void init_hashtab(struct hashtab *ht);

/* Statically initialize hashtable */
#define EMPTY_HASHTAB { 0, 0, NULL }

/* Does this key exist in the hashtable?
 * If you know that you'll never put a null value in the hashtable
 * you can just skip to `hashtab_get`. */
int hashtab_exists(struct hashtab *ht, struct string *key);
/* Get the value associated with a given key.
 * Returns NULL on missing entries. */
struct obj *hashtab_get(struct hashtab *ht, struct string *key);
/* Put `value` in the slot `key`. */
void hashtab_put(struct hashtab *ht, struct string *key, struct obj *value);

typedef void(*visit_entry)(struct string *key, struct obj *value, void *context);
/* Invoke `f` on every entry in the hashtable */
void hashtab_foreach(struct hashtab *ht, visit_entry f, void *context);