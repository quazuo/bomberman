#ifndef ROBOTS_RANDOM
#define ROBOTS_RANDOM

#include <stdint.h>

void random_start(uint32_t seed);

uint32_t random_next();

uint16_t random_pos_next(uint16_t size);

#endif //ROBOTS_RANDOM
