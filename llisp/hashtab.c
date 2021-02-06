#include <assert.h>
#include "gc.h"
#include "hashtab.h"
#include "obj.h"

#define INITIAL_HASHTAB_CAPACITY 16
#define MAX_LOAD_FACTOR 0.66
#define TOMBSTONE ((struct string *)1)

struct ht_entry {
	struct string *key;
	struct obj *value;
};

struct ht_entryarr {
	struct obj o;
	struct ht_entry entries[1];
};

void init_hashtab(struct hashtab *ht) {
	ht->size = 0;
	ht->used_slots = 0;
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

static inline _Bool entry_has_value(struct ht_entry *e) {
	return e && e->key && e->key != TOMBSTONE;
}

static struct ht_entry *hashtab_find(struct ht_entry *entries, size_t cap, struct string *key) {
	size_t target, cur;
	struct ht_entry *first_tombstone = NULL;
	if (cap == 0) return NULL;
	target = cur = jenkins_oaat_hash(key) % cap;
	do {
		struct string *curkey = entries[cur].key;
		if (curkey != TOMBSTONE && stringeq(key, curkey)) {
			/* Found the entry! */
			return &entries[cur];
		}
		if (curkey == TOMBSTONE && !first_tombstone) {
			/* Found a tombstone, we might be able to fill it in later */
			first_tombstone = &entries[cur];
		}
		if (curkey == NULL) {
			/* The real value must not exist. If we found a tombstone return that,
			 * otherwise just return this slot. */
			return first_tombstone ? first_tombstone : &entries[cur];
		}
		++cur;
		if (cur == cap) cur = 0;
	} while (cur != target);
	assert(!"Couldn't find an empty slot...");
	return first_tombstone;
}

static void hashtab_embiggen(struct hashtab *ht) {
	size_t i;
	size_t newcap = ht->cap ? ht->cap + ht->cap / 2 : INITIAL_HASHTAB_CAPACITY;
	struct ht_entryarr *newtab = (struct ht_entryarr *) gc_alloc(HASHTABARR, offsetof(struct ht_entryarr, entries) + newcap * sizeof(struct ht_entry));
	for (i = 0; i < ht->cap; ++i) {
		struct ht_entry *cur = &ht->e->entries[i];
		if (entry_has_value(cur)) {
			struct ht_entry *target = hashtab_find(newtab->entries, newcap, cur->key);
			*target = *cur;
		}
	}
	ht->cap = newcap;
	ht->used_slots = ht->size;
	ht->e = newtab;
}

void hashtab_put(struct hashtab *ht, struct string *key, struct obj *value) {
	if (ht->cap == 0 || (double)ht->used_slots / (double)ht->cap >= MAX_LOAD_FACTOR) {
		hashtab_embiggen(ht);
	}
	struct ht_entry *e = hashtab_find(ht->e->entries, ht->cap, key);
	assert(e);
	if (!e) return;
	if (e->key == NULL) {
		/* Took up another slot. */
		++ht->used_slots;
	}
	if (!entry_has_value(e)) {
		/* Whether or not we took another slot, we still added an entry. */
		++ht->size;
		e->key = key;
	}
	e->value = value;
}

_Bool hashtab_exists(struct hashtab *ht, struct string *key) {
	struct ht_entry *entry = hashtab_find(ht->e->entries, ht->cap, key);
	return entry_has_value(entry);
}
struct obj *hashtab_get(struct hashtab *ht, struct string *key) {
	struct ht_entry* entry = hashtab_find(ht->e->entries, ht->cap, key);
	return entry_has_value(entry) ? entry->value : NULL;
}

void hashtab_del(struct hashtab *ht, struct string *key) {
	struct ht_entry *entry = hashtab_find(ht->e->entries, ht->cap, key);
	if (entry_has_value(entry)) {
		--ht->size;
		entry->key = TOMBSTONE;
		/* Not strictly necessary, but safer */
		entry->value = NULL;
	}
}

void hashtab_foreach(struct hashtab *ht, visit_entry f, void *context) {
	if (ht->cap == 0) return;
	struct ht_entry *cur = ht->e->entries, *end = ht->e->entries + ht->cap;
	for (; cur != end; ++cur) {
		if (entry_has_value(cur)) {
			f(cur->key, cur->value, context);
		}
	}
}
