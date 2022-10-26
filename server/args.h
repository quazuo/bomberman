#ifndef ROBOTS_ARGS_H
#define ROBOTS_ARGS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

struct prog_args {
    char *server_name;
    uint8_t players_count;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t game_length;
    uint16_t explosion_radius;
    uint16_t bomb_timer;
    uint64_t turn_duration;
    uint16_t initial_blocks;
    uint16_t port;
    uint32_t seed;
    bool provided_seed;
    int help_flag;
};

void print_help_info(char *prog_name);

#define str_to_num(x, y, z) str_to_num__((x), (y), sizeof *(y), (z))

bool str_to_num__(char *str, void *dest, size_t size, uint64_t max);

struct prog_args parse_args(int argc, char **argv);

void free_args(struct prog_args args);

#endif //ROBOTS_ARGS_H
