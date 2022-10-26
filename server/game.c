#include "game.h"

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "utils/err.h"

struct game_state *init_state(struct prog_args *args) {
    struct game_state *state = malloc(sizeof *state);
    ENSURE(state != NULL);

    state->turn = 0;

    state->turn_bufs = malloc(args->game_length * sizeof *state->turn_bufs);
    ENSURE(state->turn_bufs != NULL);

    memset(state->player_pos, 0, sizeof state->player_pos);
    memset(state->scores, 0, sizeof state->scores);
    memset(state->is_dead, 0, sizeof state->is_dead);

    state->bombs = hmap_new();
    state->curr_bomb_id = 0;

    state->blocked = malloc(args->size_x * sizeof *state->blocked);
    ENSURE(state->blocked != NULL);
    for (int i = 0; i < args->size_x; i++) {
        state->blocked[i] = malloc(args->size_y * sizeof **state->blocked);
        ENSURE(state->blocked[i] != NULL);

        for (int j = 0; j < args->size_y; j++)
            state->blocked[i][j] = false;
    }

    return state;
}

void reset_state(struct game_state *state, struct prog_args *args) {
    state->turn = 0; // might be pointless

    for (int i = 0; i < args->game_length; i++)
        buffer_free(state->turn_bufs[i]);

    memset(state->scores, 0, sizeof(state->scores));

    hmap_free(state->bombs, true);
    state->bombs = hmap_new();
    state->curr_bomb_id = 0;

    for (int i = 0; i < args->size_x; i++) {
        for (int j = 0; j < args->size_y; j++)
            state->blocked[i][j] = false;
    }
}

void free_state(struct game_state *state, uint16_t size_x) {
    free(state->turn_bufs);
    hmap_free(state->bombs, true);
    for (int i = 0; i < size_x; i++)
        free(state->blocked[i]);
    free(state->blocked);
    free(state);
}

struct bomb_state *make_bomb(struct position pos, struct prog_args *args) {
    struct bomb_state *bomb = malloc(sizeof *bomb);
    ENSURE(bomb != NULL);

    bomb->pos = pos;
    bomb->timer = args->bomb_timer;

    return bomb;
}
