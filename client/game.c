#include "game.h"

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "utils/err.h"

struct game_state *init_state(struct msg_hello *hello) {
    struct game_state *state = malloc(sizeof *state);
    ENSURE(state != NULL);

    state->turn = 0;
    state->explosion_radius = hello->explosion_radius;
    state->bomb_timer = hello->bomb_timer;

    state->blocked = malloc(hello->size_x * sizeof *state->blocked);
    ENSURE(state->blocked != NULL);
    for (int i = 0; i < hello->size_x; i++) {
        state->blocked[i] = malloc(hello->size_y * sizeof **state->blocked);
        ENSURE(state->blocked[i] != NULL);

        for (int j = 0; j < hello->size_y; j++)
            state->blocked[i][j] = false;
    }

    state->bombs = hmap_new();

    memset(state->scores, 0, sizeof state->scores);

    return state;
}

void reset_state(struct game_state *state, struct msg_hello *hello) {
    state->turn = 0; // might be pointless

    for (int i = 0; i < hello->size_x; i++) {
        for (int j = 0; j < hello->size_y; j++)
            state->blocked[i][j] = false;
    }

    hmap_free(state->bombs, true);
    state->bombs = hmap_new();

    memset(state->scores, 0, sizeof state->scores);
}

void free_state(struct game_state *state, uint16_t size_x) {
    for (int i = 0; i < size_x; i++)
        free(state->blocked[i]);
    free(state->blocked);
    hmap_free(state->bombs, true);
    free(state);
}

void analyze_turn(struct game_state *state, struct msg_turn *turn) {
    // update the turn number
    state->turn = ntohs(turn->turn);

    struct msg_event *events = turn->event_list->arr;

    // first, update the countdowns on all bombs
    uint32_t key;
    void *value;
    hmap_it_t it = hmap_iterator(state->bombs);
    while (hmap_next(state->bombs, &it, &key, &value)) {
        struct bomb_state *curr_bomb = (struct bomb_state *) value;
        curr_bomb->timer--;
    }

    struct position pos;
    bomb_id_t bomb_id;

    // this array will let us count destroyed robots at most once
    bool destroyed[MAX_CLIENT_COUNT];
    for (int j = 0; j < MAX_CLIENT_COUNT; j++)
        destroyed[j] = false;

    // parse the events
    for (size_t i = 0; i < turn->event_list->size; i++) {
        switch (events[i].event_type) {
            case BOMB_PLACED:
                bomb_id = events[i].event_data.bomb_placed.bomb_id;
                pos = events[i].event_data.bomb_placed.pos;

                struct bomb_state *bomb = malloc(sizeof *bomb);
                ENSURE(bomb != NULL);

                bomb->pos = pos;
                bomb->timer = state->bomb_timer;
                bomb->exploded = false;

                hmap_insert(state->bombs, bomb_id, bomb);
                break;

            case BOMB_EXPLODED:
                bomb_id = events[i].event_data.bomb_exploded.bomb_id;
                struct list *robots_destroyed = events[i].event_data.bomb_exploded.robots_destroyed;

                for (size_t j = 0; j < robots_destroyed->size; j++) {
                    player_id_t id = ((player_id_t *) robots_destroyed->arr)[j];
                    if (!destroyed[id]) {
                        destroyed[id] = true;
                        state->scores[id]++;
                    }
                }

                struct bomb_state *curr_bomb = hmap_get(state->bombs, bomb_id);
                curr_bomb->exploded = true;
                break;

            case PLAYER_MOVED:;
                player_id_t id = events[i].event_data.player_moved.player_id;
                state->players[id] = events[i].event_data.player_moved.pos;
                break;

            case BLOCK_PLACED:
                pos = events[i].event_data.block_placed.pos;
                pos.x = ntohs(pos.x);
                pos.y = ntohs(pos.y);
                state->blocked[pos.x][pos.y] = true;
                break;
        }
    }

    for (size_t i = 0; i < turn->event_list->size; i++) {
        if (events[i].event_type == BOMB_EXPLODED) {
            struct event_bomb_exploded event_data = events[i].event_data.bomb_exploded;
            free(event_data.robots_destroyed->arr);
            free(event_data.robots_destroyed);
            free(event_data.blocks_destroyed->arr);
            free(event_data.blocks_destroyed);
        }
    }
    free(events);
    free(turn->event_list);
}
