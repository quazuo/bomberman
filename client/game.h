#ifndef ROBOTS_GAME
#define ROBOTS_GAME

#include <stdbool.h>

#include "msg.h"
#include "utils/hmap.h"

struct bomb_state {
    struct position pos;
    uint16_t timer;
    bool exploded;
};

struct game_state {
    struct position players[MAX_CLIENT_COUNT];
    score_t scores[MAX_CLIENT_COUNT];
    uint16_t turn;
    bool **blocked;
    hmap_t *bombs;
    uint16_t explosion_radius;
    uint16_t bomb_timer;
};

struct game_state *init_state(struct msg_hello *hello);

void free_state(struct game_state *state, uint16_t size_x);

void reset_state(struct game_state *state, struct msg_hello *hello);

void analyze_turn(struct game_state *state, struct msg_turn *turn);

#endif