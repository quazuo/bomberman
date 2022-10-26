#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <poll.h>
#include <arpa/inet.h>
#include <time.h>
#include <limits.h>

#include "utils/random.h"
#include "utils/buffer.h"
#include "utils/err.h"
#include "net.h"
#include "game.h"
#include "msg.h"
#include "args.h"

enum {
    LOBBY,
    GAME
} server_state = LOBBY;

enum {
    PLAYER,
    SPECTATOR
} clients[N_FDS];

void start_game(struct game_state *state, struct prog_args *args) {
    reset_state(state, args);
    buffer_t *temp = buffer_new();

    list_len_t events_count = args->players_count;
    msg_type_t msg_type = PLAYER_MOVED;

    for (player_id_t id = 0; id < args->players_count; id++) {
        buffer_push(temp, &msg_type, sizeof msg_type);
        buffer_push(temp, &id, sizeof id);

        struct position pos;
        pos.x = random_pos_next(args->size_x);
        pos.y = random_pos_next(args->size_y);
        state->player_pos[id] = pos;

        pos.x = htons(pos.x);
        pos.y = htons(pos.y);
        buffer_push(temp, &pos, sizeof pos);
    }

    msg_type = BLOCK_PLACED;

    for (int i = 0; i < args->initial_blocks; i++) {
        struct position pos;
        pos.x = random_pos_next(args->size_x);
        pos.y = random_pos_next(args->size_y);

        if (!state->blocked[pos.x][pos.y]) {
            events_count++;

            buffer_push(temp, &msg_type, sizeof msg_type);

            state->blocked[pos.x][pos.y] = true;
            pos.x = htons(pos.x);
            pos.y = htons(pos.y);
            buffer_push(temp, &pos, sizeof pos);
        }
    }

    buffer_t *buffer = buffer_new();
    events_count = htonl(events_count);
    buffer_push(buffer, &events_count, sizeof events_count);
    buffer_push(buffer, temp->buf, temp->size);
    buffer_free(temp);

    state->turn_bufs[0] = buffer;
    state->turn = 1;
}

void analyze_bombs(struct game_state *state, struct prog_args *args) {
    buffer_t *events_temp = buffer_new(); // buffer for the events
    buffer_t *robots_temp = buffer_new(); // buffer for the `robots_destroyed` list
    buffer_t *blocks_temp = buffer_new(); // buffer for the `blocks_destroyed` list

    // destroyed[x][y] == "was the block (x,y) destroyed this turn?"
    bool **block_destr = malloc(args->size_x * sizeof *block_destr);
    ENSURE(block_destr != NULL);
    for (int i = 0; i < args->size_x; i++) {
        block_destr[i] = malloc(args->size_y * sizeof **block_destr);
        ENSURE(block_destr[i] != NULL);
        for (int j = 0; j < args->size_y; j++)
            block_destr[i][j] = false;
    }

    list_len_t list_len = 0;
    bomb_id_t key;
    void *value;
    hmap_it_t it = hmap_iterator(state->bombs);

    while (hmap_next(state->bombs, &it, &key, &value)) {
        struct bomb_state *curr_bomb = (struct bomb_state *) value;

        curr_bomb->timer--;
        if (curr_bomb->timer == 0) {
            // coordinates of the bomb
            uint16_t x = curr_bomb->pos.x;
            uint16_t y = curr_bomb->pos.y;
            int dx = 0, dy = 1; // vector

            // insert first data about the new `BombExploded` event into the buffer
            list_len++;
            msg_type_t msg_type = BOMB_EXPLODED;
            buffer_push(events_temp, &msg_type, sizeof msg_type);
            bomb_id_t net_bomb_id = htonl(key);
            buffer_push(events_temp, &net_bomb_id, sizeof net_bomb_id);

            list_len_t robots_count = 0, blocks_count = 0;

            hmap_remove(state->bombs, key, true);

            // check for destroyed robots on the bomb's tile
            for (player_id_t id = 0; id < args->players_count; id++) {
                if (state->is_dead[id]) // skip already destroyed robots
                    continue;

                if (state->player_pos[id].x == x && state->player_pos[id].y == y) {
                    state->is_dead[id] = true;
                    robots_count++;
                    buffer_push(robots_temp, &id, sizeof id);
                }
            }

            if (state->blocked[x][y]) { // bomb exploded on a blocked square
                // add the destroyed block
                block_destr[x][y] = true;
                blocks_count++;
                struct position net_pos = {htons(x), htons(y)};
                buffer_push(blocks_temp, &net_pos, sizeof net_pos);

            } else { // bomb exploded on a free square
                for (int i = 0; i < 4; i++) {
                    for (int j = 1; j <= args->explosion_radius; j++) {
                        // coordinates of a square in the range of an explosion
                        int xx = x + j * dx;
                        int yy = y + j * dy;

                        // check if it's out of bounds
                        if (xx < 0 || xx >= args->size_x || yy < 0 || yy >= args->size_y)
                            break;

                        // check for destroyed robots
                        for (player_id_t id = 0; id < args->players_count; id++) {
                            if (state->is_dead[id]) // skip already destroyed robots
                                continue;

                            if (state->player_pos[id].x == xx && state->player_pos[id].y == yy) {
                                state->is_dead[id] = true;
                                robots_count++;
                                buffer_push(robots_temp, &id, sizeof id);
                            }
                        }

                        // check if a block was destroyed
                        if (state->blocked[xx][yy]) {
                            block_destr[xx][yy] = true;
                            blocks_count++;
                            struct position net_pos = {htons((uint16_t) xx),
                                                       htons((uint16_t) yy)};
                            buffer_push(blocks_temp, &net_pos, sizeof net_pos);
                            break;
                        }
                    }

                    // rotate the vector by 90 degrees clockwise
                    int temp = dx;
                    dx = dy;
                    dy = -temp;
                }
            }

            // append the `robots_destroyed` list
            robots_count = htonl(robots_count);
            buffer_push(events_temp, &robots_count, sizeof robots_count);
            buffer_push(events_temp, robots_temp->buf, robots_temp->size);

            // append the `blocks_destroyed` list
            blocks_count = htonl(blocks_count);
            buffer_push(events_temp, &blocks_count, sizeof blocks_count);
            buffer_push(events_temp, blocks_temp->buf, blocks_temp->size);

            buffer_clear(robots_temp);
            buffer_clear(blocks_temp);
        }
    }

    // process the `destroyed` array
    for (int i = 0; i < args->size_x; i++) {
        for (int j = 0; j < args->size_y; j++) {
            if (block_destr[i][j])
                state->blocked[i][j] = false;
        }
    }

    // process the `is_dead` array
    for (player_id_t id = 0; id < args->players_count; id++) {
        if (state->is_dead[id]) {
            struct position new_pos = {random_pos_next(args->size_x),
                                       random_pos_next(args->size_y)};
            state->player_pos[id] = new_pos;

            msg_type_t msg_type = PLAYER_MOVED;
            buffer_push(events_temp, &msg_type, sizeof msg_type);

            buffer_push(events_temp, &id, sizeof id);

            new_pos.x = htons(new_pos.x);
            new_pos.y = htons(new_pos.y);
            buffer_push(events_temp, &new_pos, sizeof new_pos);

            list_len++;
        }
    }

    buffer_t *buffer = buffer_new();

    // not in net byte order! will be used by `analyze_actions()`
    buffer_push(buffer, &list_len, sizeof list_len);

    buffer_push(buffer, events_temp->buf, events_temp->size);

    state->turn_bufs[state->turn] = buffer;

    buffer_free(events_temp);
    buffer_free(robots_temp);
    buffer_free(blocks_temp);
    for (int i = 0; i < args->size_x; i++)
        free(block_destr[i]);
    free(block_destr);
}

void analyze_actions(struct game_state *state, struct prog_args *args) {
    buffer_t *buffer = state->turn_bufs[state->turn];
    list_len_t list_len;
    memcpy(&list_len, buffer->buf, sizeof list_len); // pull events count from the buffer

    msg_type_t msg_type;

    for (player_id_t id = 0; id < args->players_count; id++) {
        switch (state->actions[id].type) {
            case PLACE_BOMB:;
                struct bomb_state *bomb = make_bomb(state->player_pos[id], args);
                hmap_insert(state->bombs, state->curr_bomb_id, bomb);

                msg_type = BOMB_PLACED;
                buffer_push(buffer, &msg_type, sizeof msg_type);

                bomb_id_t net_bomb_id = htonl(state->curr_bomb_id);
                buffer_push(buffer, &net_bomb_id, sizeof net_bomb_id);

                struct position net_bomb_pos = {htons(bomb->pos.x), htons(bomb->pos.y)};
                buffer_push(buffer, &net_bomb_pos, sizeof net_bomb_pos);

                state->curr_bomb_id++;
                list_len++;
                break;

            case PLACE_BLOCK:;
                struct position pos = state->player_pos[id];
                state->blocked[pos.x][pos.y] = true;

                msg_type = BLOCK_PLACED;
                buffer_push(buffer, &msg_type, sizeof msg_type);

                struct position net_block_pos = state->player_pos[id];
                net_block_pos.x = htons(net_block_pos.x);
                net_block_pos.y = htons(net_block_pos.y);
                buffer_push(buffer, &net_block_pos, sizeof net_block_pos);

                list_len++;
                break;

            case MOVE:
                if (state->is_dead[id])
                    break;

                int vec_x = 0, vec_y = 1;

                // rotate the vector until we get the deserved result
                for (int i = 0; i < state->actions[id].direction; i++) {
                    int temp = vec_x;
                    vec_x = vec_y;
                    vec_y = -temp;
                }

                // construct new player position
                int32_t new_x = state->player_pos[id].x + vec_x;
                int32_t new_y = state->player_pos[id].y + vec_y;

                // check if out of bounds
                if (new_x < 0 || new_x >= args->size_x
                    || new_y < 0 || new_y >= args->size_y)
                    break;

                // check if trying to walk onto a block
                if (state->blocked[new_x][new_y])
                    break;

                struct position new_pos = {(uint16_t) new_x, (uint16_t) new_y};
                state->player_pos[id] = new_pos;

                msg_type = PLAYER_MOVED;
                buffer_push(buffer, &msg_type, sizeof msg_type);

                buffer_push(buffer, &id, sizeof id);

                new_pos.x = htons(new_pos.x);
                new_pos.y = htons(new_pos.y);
                buffer_push(buffer, &new_pos, sizeof new_pos);

                list_len++;
                break;

            case NONE:
            default:
                break;
        }
    }

    list_len = htonl(list_len);
    memcpy(buffer->buf, &list_len, sizeof list_len);
}

void analyze_turn(struct game_state *game_state, struct prog_args *args) {
    analyze_bombs(game_state, args);
    analyze_actions(game_state, args);

    // update scores
    for (player_id_t id = 0; id < args->players_count; id++)
        game_state->scores[id] += game_state->is_dead[id] ? 1 : 0;

    // clear all temporary data
    memset(game_state->actions, 0, sizeof game_state->actions);
    memset(game_state->is_dead, 0, sizeof game_state->is_dead);
}

uint64_t get_passed_ms(struct timespec *spec) {
    struct timespec spec_now;
    clock_gettime(CLOCK_MONOTONIC, &spec_now);
    uint64_t old_time = (uint64_t) (spec->tv_sec * 1000 + spec->tv_nsec / 1000000);
    uint64_t new_time = (uint64_t) (spec_now.tv_sec * 1000 + spec_now.tv_nsec / 1000000);
    return new_time - old_time;
}

int main(int argc, char **argv) {
    struct prog_args args = parse_args(argc, argv);

    if (args.help_flag) {
        free_args(args);
        print_help_info(argv[0]);
        return 0;
    }

    // start the RNG
    if (args.provided_seed)
        random_start(args.seed);
    else
        random_start((uint32_t) time(NULL));

    // pre-build the `Hello` message
    buffer_t *hello_buf = buffer_new();
    struct msg_hello hello = build_hello(args);

    serialize_hello(hello_buf, hello);

    // initialize the state variables
    struct msg_player players[N_FDS]; // index in here and index in `fds[]` are the same, thus using N_FDS
    memset(players, 0, sizeof players);

    struct game_state *game_state = init_state(&args);

    for (int i = 0; i < N_FDS; i++)
        clients[i] = SPECTATOR;

    int n_players = 0;

    // timer for turns
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);

    // prepare socket
    int my_fd = bind_socket_tcp(args.port);
    listen(my_fd, QUEUE_LEN);

    // `poll()` setup
    struct pollfd fds[N_FDS];

    for (int i = 0; i < N_FDS; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }

    fds[0].fd = my_fd;

    while (true) {
        if (server_state == LOBBY) {
            poll(fds, N_FDS, -1); // wait indefinitely

        } else { // server_state == GAME
            uint64_t passed_ms = get_passed_ms(&spec);
            if (passed_ms < args.turn_duration) {
                if (args.turn_duration - passed_ms < INT_MAX)
                    poll(fds, N_FDS, (int) (args.turn_duration - passed_ms));
                else
                    poll(fds, N_FDS, INT_MAX);
            }
        }

        // check if turn has ended
        uint64_t passed_ms = get_passed_ms(&spec);

        // turn ended, time to parse all the data and move on to the next turn
        if (server_state == GAME && passed_ms > args.turn_duration) {
            analyze_turn(game_state, &args);

            // send `Turn` to all
            for (int i = 1; i < N_FDS; i++) {
                if (fds[i].fd != -1)
                    send_turn(fds[i].fd, game_state->turn_bufs[game_state->turn], game_state->turn);
            }

            // check if the game has ended
            if (game_state->turn == args.game_length) {
                buffer_t *game_ended_buf =
                    build_game_ended(game_state->scores, args.players_count);

                for (int i = 1; i < N_FDS; i++) {
                    if (fds[i].fd != -1)
                        send(fds[i].fd, game_ended_buf->buf, game_ended_buf->size, 0);
                }

                server_state = LOBBY;
                for (int i = 0; i < N_FDS; i++)
                    clients[i] = SPECTATOR;
                n_players = 0;
            }

            game_state->turn++;

            // reset timer
            clock_gettime(CLOCK_MONOTONIC, &spec);
        }

        if (fds[0].revents & POLLIN) { // new connection
            fds[0].revents = 0;

            for (int i = 1; i < N_FDS; i++) {
                if (fds[i].fd == -1) {
                    accept_client(my_fd, &fds[i].fd, &players[i]);

                    // immediately send `Hello` to the newly connected client
                    send_hello(fds[i].fd, hello_buf);

                    // send `AcceptedPlayer` messages if we're in a lobby
                    if (server_state == LOBBY) {
                        for (int j = 1; j < N_FDS; j++) {
                            if (clients[j] == PLAYER)
                                send_accepted_player(fds[i].fd, &players[j]);
                        }

                    } else { // server_state == GAME
                        send_game_started(fds[i].fd, players, args.players_count);
                        send_turns_recap(fds[i].fd, game_state->turn_bufs, game_state->turn);
                    }

                    break;
                }
            }
        }

        for (int i = 1; i < N_FDS; i++) { // a client sent something
            if (fds[i].revents & (POLLERR | POLLHUP)) {
                disconnect_client(&fds[i].fd);

            } else if (fds[i].revents & POLLIN) {
                fds[i].revents = 0;

                msg_type_t msg_type;
                recv_check(&fds[i].fd, &msg_type, sizeof(msg_type));

                switch (msg_type) {
                    case JOIN:;
                        // read it even if `server_state == GAME`, because
                        // we don't want stale data in the socket's buffer.
                        char *name = parse_string(&fds[i].fd);


                        if (server_state == GAME)
                            break;

                        if (clients[i] == PLAYER)
                            break;

                        players[i].name = name;
                        players[i].id = (player_id_t) n_players;
                        n_players++;
                        clients[i] = PLAYER;

                        for (int j = 1; j < N_FDS; j++) {
                            if (fds[j].fd != -1)
                                send_accepted_player(fds[j].fd, &players[i]);
                        }

                        // if enough players signed up, start the game
                        if (n_players == args.players_count) {
                            server_state = GAME;
                            start_game(game_state, &args);

                            for (int j = 1; j < N_FDS; j++) {
                                if (fds[j].fd != -1) {
                                    send_game_started(fds[j].fd, players, args.players_count);
                                    send_turn(fds[j].fd, game_state->turn_bufs[0], 0);
                                }
                            }

                            clock_gettime(CLOCK_MONOTONIC, &spec);
                        }
                        break;

                    case PLACE_BLOCK:
                    case PLACE_BOMB:
                    case MOVE:
                        if (clients[i] == SPECTATOR)
                            parse_action(&fds[i].fd, msg_type);
                        else
                            game_state->actions[players[i].id] = parse_action(&fds[i].fd, msg_type);

                        if (game_state->actions[players[i].id].type == ERR)
                            disconnect_client(&fds[i].fd);
                        break;

                    default:
                        disconnect_client(&fds[i].fd);
                        break;
                }
            }
        }
    }
}
