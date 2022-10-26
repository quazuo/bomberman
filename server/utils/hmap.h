#ifndef ROBOTS_HMAP
#define ROBOTS_HMAP

#include <stdbool.h>
#include <sys/types.h>
#include <stdint.h>

typedef struct HashMap hmap_t;

hmap_t* hmap_new();

void hmap_free(hmap_t* map, bool value_is_alloc);

void* hmap_get(hmap_t* map, uint32_t key);

bool hmap_insert(hmap_t* map, uint32_t key, void* value);

bool hmap_remove(hmap_t* map, uint32_t key, bool value_is_alloc);

typedef struct HashMapIterator {
    int bucket;
    void* pair;
} hmap_it_t;

hmap_it_t hmap_iterator(hmap_t* map);

// Set `*key` and `*value` to the current element pointed by iterator and
// move the iterator to the next element.
// If there are no more elements, leaves `*key` and `*value` unchanged and
// returns false.
//
// The map cannot be modified between calls to `hmap_iterator` and `hmap_next`.
//
// Usage: ```
//     const char* key;
//     void* value;
//     HashMapIterator it = hmap_iterator(map);
//     while (hmap_next(map, &it, &key, &value))
//         foo(key, value);
// ```
bool hmap_next(hmap_t* map, hmap_it_t* it, uint32_t *key, void** value);

#endif // ROBOTS_HMAP