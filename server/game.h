#ifndef ROBOTS_GAME
#define ROBOTS_GAME

#include <stdbool.h>

#include "utils/hmap.h"
#include "msg.h"
#include "net.h"
#include "args.h"

struct bomb_state {
    struct position pos;
    uint16_t timer;
};

struct game_state {
    uint16_t turn;
    buffer_t **turn_bufs;

    struct position player_pos[MAX_CLIENT_COUNT];
    struct msg_action actions[MAX_CLIENT_COUNT];
    score_t scores[MAX_CLIENT_COUNT];
    bool is_dead[MAX_CLIENT_COUNT];

    hmap_t *bombs;
    bomb_id_t curr_bomb_id;
    bool **blocked;
};

struct game_state *init_state(struct prog_args *args);

void free_state(struct game_state *state, uint16_t size_x);

void reset_state(struct game_state *state, struct prog_args *args);

player_id_t find_player(uint16_t x, uint16_t y, struct game_state *state, uint8_t players_count);

struct bomb_state *make_bomb(struct position pos, struct prog_args *args);

#endif