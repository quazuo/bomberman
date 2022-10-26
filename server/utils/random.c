#include "random.h"

static uint32_t previous;

void random_start(uint32_t seed) {
    previous = seed;
}

uint32_t random_next() {
    uint32_t result = (uint32_t) (((uint64_t) previous * 48271) % 2147483647);
    previous = result;
    return result;
}

uint16_t random_pos_next(uint16_t size) {
    return (uint16_t) (random_next() % size);
}
