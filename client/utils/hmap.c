#include "hmap.h"

#include <stdlib.h>
#include <string.h>

#include "../msg.h"
#include "err.h"

#define N_BUCKETS 31

typedef struct Pair {
    uint32_t key;
    void *value;
    struct Pair *next;
} Pair;

struct HashMap {
    Pair *buckets[N_BUCKETS];
    size_t size;
};

static unsigned int get_hash(uint32_t key);

hmap_t *hmap_new() {
    hmap_t *map = malloc(sizeof(hmap_t));
    if (!map)
        return NULL;

    memset(map, 0, sizeof(hmap_t));
    return map;
}

void hmap_free(hmap_t *map, bool value_is_alloc) {
    for (int h = 0; h < N_BUCKETS; ++h) {
        for (Pair *p = map->buckets[h]; p;) {
            Pair *q = p;
            p = p->next;
            if (value_is_alloc)
                free(q->value);
            free(q);
        }
    }
    free(map);
}

static Pair *hmap_find(hmap_t *map, unsigned int h, uint32_t key) {
    for (Pair *p = map->buckets[h]; p; p = p->next) {
        if (key == p->key) {
            return p;
        }
    }

    return NULL;
}

void *hmap_get(hmap_t *map, uint32_t key) {
    uint32_t h = get_hash(key);
    Pair *p = hmap_find(map, h, key);
    return p ? p->value : NULL;
}

bool hmap_insert(hmap_t *map, uint32_t key, void *value) {
    if (!value)
        return false;

    uint32_t h = get_hash(key);
    if (hmap_find(map, h, key))
        return false;

    Pair *new_p = malloc(sizeof(Pair));
    new_p->key = key;
    new_p->value = value;
    new_p->next = map->buckets[h];
    map->buckets[h] = new_p;
    map->size++;

    return true;
}

bool hmap_remove(hmap_t *map, uint32_t key, bool value_is_alloc) {
    uint32_t h = get_hash(key);
    Pair **pp = &(map->buckets[h]);

    while (*pp) {
        Pair *p = *pp;

        if (key == p->key) {
            *pp = p->next;
            if (value_is_alloc)
                free(p->value);
            free(p);
            map->size--;
            return true;
        }

        pp = &(p->next);
    }

    return false;
}

hmap_it_t hmap_iterator(hmap_t *map) {
    hmap_it_t it = {0, map->buckets[0]};
    return it;
}

bool hmap_next(hmap_t *map, hmap_it_t *it, uint32_t *key, void **value) {
    Pair *p = it->pair;
    while (!p && it->bucket < N_BUCKETS - 1) {
        p = map->buckets[++it->bucket];
    }
    if (!p)
        return false;
    *key = p->key;
    *value = p->value;
    it->pair = p->next;
    return true;
}

static unsigned int get_hash(uint32_t key) {
    return (key * 5) % N_BUCKETS;
}
