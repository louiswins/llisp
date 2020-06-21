#include <assert.h>
#include "gc.h"
#include "hashtab.h"
#include "obj.h"

#define INITIAL_HASHTAB_CAPACITY 16
#define MAX_LOAD_FACTOR 0.66

struct ht_entry {
	struct string *key;
	struct obj *value;
};

struct ht_entryarr {
	struct gc_head gc;
	struct ht_entry entries[1];
};

void init_hashtab(struct hashtab *ht) {
	ht->size = 0;
	ht->cap = 0;
	ht->e = NULL;
}

/* Implementation of Bob Jenkins's one-at-a-time hash taken from
 * https://en.wikipedia.org/wiki/Jenkins_hash_function */
uint32_t jenkins_oaat_hash(struct string *key) {
	char *cur = key->str;
	char *end = cur + key->len;
	uint32_t hash = 0;
	while (cur != end) {
		hash += *cur++;
		hash += hash << 10;
		hash ^= hash >> 6;
	}
	hash += hash << 3;
	hash ^= hash >> 11;
	hash += hash << 15;
	return hash;
}

static struct ht_entry *hashtab_find(struct ht_entry *entries, size_t cap, struct string *key) {
	size_t target, cur;
	if (cap == 0) return NULL;
	target = cur = jenkins_oaat_hash(key) % cap;
	do {
		struct string *curkey = entries[cur].key;
		if (curkey == NULL || stringeq(key, curkey)) {
			return &entries[cur];
		}
		++cur;
		if (cur == cap) cur = 0;
	} while (cur != target);
	assert(!"Couldn't find an empty slot...");
	return NULL;
}

static void hashtab_embiggen(struct hashtab *ht) {
	size_t i;
	size_t newcap = ht->cap ? ht->cap + ht->cap / 2 : INITIAL_HASHTAB_CAPACITY;
	struct ht_entryarr *newtab = gc_alloc(HASHTABARR, offsetof(struct ht_entryarr, entries) + newcap * sizeof(struct ht_entry));
	for (i = 0; i < ht->cap; ++i) {
		struct string *curkey = ht->e->entries[i].key;
		if (curkey != NULL) {
			struct ht_entry *target = hashtab_find(newtab->entries, newcap, curkey);
			target->key = curkey;
			target->value = ht->e->entries[i].value;
		}
	}
	ht->cap = newcap;
	ht->e = newtab;
}

void hashtab_put(struct hashtab *ht, struct string *key, struct obj *value) {
	if (ht->cap == 0 || (double)ht->size / (double)ht->cap >= MAX_LOAD_FACTOR) {
		hashtab_embiggen(ht);
	}
	struct ht_entry *e = hashtab_find(ht->e->entries, ht->cap, key);
	if (e->key == NULL) {
		++ht->size;
		e->key = key;
	}
	e->value = value;
}

int hashtab_exists(struct hashtab *ht, struct string *key) {
	struct ht_entry* entry = hashtab_find(ht->e->entries, ht->cap, key);
	return entry && entry->key;
}
struct obj *hashtab_get(struct hashtab *ht, struct string *key) {
	struct ht_entry* entry = hashtab_find(ht->e->entries, ht->cap, key);
	return entry ? entry->value : NULL;
}

void hashtab_foreach(struct hashtab *ht, visit_entry f, void *context) {
	if (ht->cap == 0) return;
	struct ht_entry *cur = ht->e->entries, *end = ht->e->entries + ht->cap;
	for (; cur != end; ++cur) {
		if (cur->key != NULL) {
			f(cur->key, cur->value, context);
		}
	}
}
