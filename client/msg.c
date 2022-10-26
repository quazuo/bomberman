#include "msg.h"

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <stdint.h>

#include "utils/err.h"
#include "utils/buffer.h"
#include "utils/hmap.h"
#include "game.h"
#include "net.h"

/** ******************************************************** */
/**                    Deserialization                       */
/** ******************************************************** */

static char *parse_string(int sockfd) {
    str_len_t len;
    recv(sockfd, &len, sizeof len, MSG_WAITALL);

    char *str = malloc((sizeof len + len) * sizeof *str);
    ENSURE(str != NULL);
    memcpy(str, &len, sizeof len);
    recv(sockfd, str + sizeof len, len, MSG_WAITALL);

    return str;
}

struct msg_hello parse_hello(int sockfd) {
    struct msg_hello hello;
    hello.server_name = parse_string(sockfd);

    // copy the rest of the message into the packed struct
    size_t rest_offset = sizeof hello.server_name; // offset at which the numeric data starts
    size_t rest_size = sizeof(struct msg_hello) - sizeof hello.server_name;
    recv(sockfd, (char *) &hello + rest_offset, rest_size, MSG_WAITALL);

    hello.size_x = ntohs(hello.size_x);
    hello.size_y = ntohs(hello.size_y);
    hello.game_length = ntohs(hello.game_length);
    hello.explosion_radius = ntohs(hello.explosion_radius);
    hello.bomb_timer = ntohs(hello.bomb_timer);

    return hello;
}

struct msg_player parse_player(int sockfd) {
    struct msg_player player;

    recv(sockfd, &player.id, sizeof player.id, MSG_WAITALL);
    player.name = parse_string(sockfd);
    player.address = parse_string(sockfd);

    return player;
}

void parse_game_started(int sockfd, struct msg_player players[]) {
    map_len_t players_count;
    recv(sockfd, &players_count, sizeof players_count, MSG_WAITALL);
    players_count = ntohl(players_count);

    for (map_len_t i = 0; i < players_count; i++) {
        struct msg_player curr_player = parse_player(sockfd);
        players[curr_player.id] = curr_player;
    }
}

struct msg_turn parse_turn(int sockfd) {
    uint16_t turn;
    recv(sockfd, &turn, sizeof turn, MSG_WAITALL);

    list_len_t event_count;
    recv(sockfd, &event_count, sizeof event_count, MSG_WAITALL);
    event_count = ntohl(event_count);

    struct msg_event *events = malloc(event_count * sizeof *events);
    ENSURE(events != NULL);

    struct list *events_list = malloc(sizeof *events_list);
    ENSURE(events_list != NULL);
    events_list->size = event_count;
    events_list->arr = events;

    for (list_len_t i = 0; i < event_count; i++) {
        recv(sockfd, &events[i].event_type, sizeof events[i].event_type, MSG_WAITALL);

        switch (events[i].event_type) {
            case BOMB_PLACED:
                recv(sockfd, &events[i].event_data.bomb_placed,
                     sizeof events[i].event_data.bomb_placed, MSG_WAITALL);
                break;

            case PLAYER_MOVED:
                recv(sockfd, &events[i].event_data.player_moved,
                     sizeof events[i].event_data.player_moved, MSG_WAITALL);
                break;

            case BLOCK_PLACED:
                recv(sockfd, &events[i].event_data.block_placed,
                     sizeof events[i].event_data.block_placed, MSG_WAITALL);
                break;

            case BOMB_EXPLODED:
                recv(sockfd, &events[i].event_data.bomb_exploded.bomb_id,
                     sizeof events[i].event_data.bomb_exploded.bomb_id, MSG_WAITALL);

                // get the `robots_destroyed` list
                list_len_t list_len = 0;
                recv(sockfd, &list_len, sizeof list_len, MSG_WAITALL);
                list_len = ntohl(list_len);

                struct list *robots_destroyed = malloc(sizeof(struct list));
                ENSURE(robots_destroyed != NULL);
                robots_destroyed->size = list_len;
                robots_destroyed->arr = malloc(list_len * sizeof(player_id_t));
                if (list_len > 0)
                    ENSURE(robots_destroyed->arr != NULL);

                recv(sockfd, robots_destroyed->arr,
                     list_len * sizeof(player_id_t), MSG_WAITALL);

                events[i].event_data.bomb_exploded.robots_destroyed = robots_destroyed;

                // get the `blocks_destroyed` list
                recv(sockfd, &list_len, sizeof list_len, MSG_WAITALL);
                list_len = ntohl(list_len);

                struct list *blocks_destroyed = malloc(sizeof(struct list));
                ENSURE(blocks_destroyed != NULL);
                blocks_destroyed->size = list_len;
                blocks_destroyed->arr = malloc(list_len * sizeof(struct position));
                if (list_len > 0)
                    ENSURE(blocks_destroyed->arr != NULL);

                recv(sockfd, blocks_destroyed->arr,
                     list_len * sizeof(struct position), MSG_WAITALL);

                events[i].event_data.bomb_exploded.blocks_destroyed = blocks_destroyed;

                break;

            default:
                fatal("Invalid event type");
        }
    }

    struct msg_turn result = {turn, events_list};
    return result;
}

struct msg_score *parse_game_ended(int sockfd) {
    map_len_t scores_count;
    recv(sockfd, &scores_count, sizeof scores_count, MSG_WAITALL);
    scores_count = ntohl(scores_count);

    struct msg_score *scores = malloc(scores_count * sizeof *scores);
    ENSURE(scores != NULL);
    recv(sockfd, scores, scores_count * sizeof *scores, MSG_WAITALL);

    return scores;
}

struct msg_input parse_input(int sockfd) {
    struct msg_input input;
    char buffer[sizeof input + 1]; // a little bit more than we need to read

    ssize_t read_len = recv(sockfd, buffer, sizeof buffer, 0);
    if (read_len == -1 || (size_t) read_len > sizeof input)
        input.type = GUI_ERR;

    memcpy(&input, buffer, sizeof input);

    if (input.type > GUI_MOVE)
        input.type = GUI_ERR;
    if (input.type == GUI_MOVE && read_len != 2)
        input.type = GUI_ERR;
    if ((input.type == GUI_PLACE_BOMB || input.type == GUI_PLACE_BLOCK) && read_len != 1)
        input.type = GUI_ERR;

    return input;
}

/** ******************************************************** */
/**                      Serialization                       */
/** ******************************************************** */

static void serialize_player(buffer_t *buffer, struct msg_player *player) {
    buffer_push(buffer, &player->id, sizeof player->id);

    str_len_t strlen = (str_len_t) player->name[0];
    buffer_push(buffer, player->name, strlen + 1);

    strlen = (str_len_t) player->address[0];
    buffer_push(buffer, player->address, strlen + 1);
}

static void serialize_blocks(buffer_t *buffer, struct game_state *state, uint16_t size_x, uint16_t size_y) {
    buffer_t *blocks_buf = buffer_new();

    list_len_t list_len = 0;
    for (uint16_t i = 0; i < size_x; i++) {
        for (uint16_t j = 0; j < size_y; j++) {
            if (!state->blocked[i][j])
                continue;

            list_len++;
            struct position pos = {htons(i), htons(j)};
            buffer_push(blocks_buf, &pos, sizeof pos);
        }
    }

    list_len = htonl(list_len);
    buffer_push(buffer, &list_len, sizeof list_len);
    buffer_push(buffer, blocks_buf->buf, blocks_buf->size);
    buffer_free(blocks_buf);
}

static void serialize_bombs(buffer_t *buffer, struct game_state *state) {
    buffer_t *bombs_buf = buffer_new();

    list_len_t list_len = 0;
    uint32_t key;
    void *value;
    hmap_it_t it = hmap_iterator(state->bombs);

    while (hmap_next(state->bombs, &it, &key, &value)) {
        struct bomb_state *curr_bomb = (struct bomb_state *) value;
        if (!curr_bomb->exploded) {
            buffer_push(bombs_buf, &curr_bomb->pos, sizeof curr_bomb->pos);
            uint16_t timer = htons(curr_bomb->timer);
            buffer_push(bombs_buf, &timer, sizeof timer);
            list_len++;
        }
    }

    list_len = htonl(list_len);
    buffer_push(buffer, &list_len, sizeof list_len);
    buffer_push(buffer, bombs_buf->buf, bombs_buf->size);
    buffer_free(bombs_buf);
}

void serialize_explosions(buffer_t *buffer, struct game_state *state, uint16_t size_x, uint16_t size_y) {
    buffer_t *exp_buf = buffer_new();

    bool **explosions = malloc(size_x * sizeof *explosions);
    ENSURE(explosions != NULL);
    for (int i = 0; i < size_x; i++) {
        explosions[i] = malloc(size_y * sizeof **explosions);
        ENSURE(explosions[i] != NULL);
        for (int j = 0; j < size_y; j++)
            explosions[i][j] = false;
    }

    uint32_t key;
    void *value;
    hmap_it_t it = hmap_iterator(state->bombs);

    while (hmap_next(state->bombs, &it, &key, &value)) {
        struct bomb_state *curr_bomb = (struct bomb_state *) value;

        if (curr_bomb->exploded) {
            int x = ntohs(curr_bomb->pos.x);
            int y = ntohs(curr_bomb->pos.y);
            int dx = 0, dy = 1; // vector

            hmap_remove(state->bombs, key, true);
            explosions[x][y] = true;

            if (state->blocked[x][y])
                continue;

            for (int i = 0; i < 4; i++) {
                for (int j = 1; j <= state->explosion_radius; j++) {
                    if (x + j * dx < 0 || x + j * dx >= size_x ||
                        y + j * dy < 0 || y + j * dy >= size_y)
                        break;

                    explosions[x + j * dx][y + j * dy] = true;
                    if (state->blocked[x + j * dx][y + j * dy]) {
                        break;
                    }
                }

                // rotate the vector by 90 degrees clockwise
                int temp = dx;
                dx = dy;
                dy = -temp;
            }
        }
    }

    // populate the buffer
    list_len_t list_len = 0;

    for (uint16_t i = 0; i < size_x; i++) {
        for (uint16_t j = 0; j < size_y; j++) {
            if (explosions[i][j]) {
                state->blocked[i][j] = false;
                struct position pos = {htons(i), htons(j)};
                buffer_push(exp_buf, &pos, sizeof pos);
                list_len++;
            }
        }
    }

    list_len = htonl(list_len);
    buffer_push(buffer, &list_len, sizeof list_len);
    buffer_push(buffer, exp_buf->buf, exp_buf->size);
    buffer_free(exp_buf);

    for (int i = 0; i < size_x; i++)
        free(explosions[i]);
    free(explosions);
}

void serialize_scores(buffer_t *buffer, struct game_state *state,
                      struct msg_player players[], uint8_t players_count) {
    map_len_t scores_len = htonl(players_count);
    buffer_push(buffer, &scores_len, sizeof scores_len);

    for (uint8_t i = 0; i < players_count; i++) {
        uint8_t curr_id = players[i].id;
        buffer_push(buffer, &curr_id, sizeof curr_id);
        score_t curr_score = htonl(state->scores[curr_id]);
        buffer_push(buffer, &curr_score, sizeof curr_score);
    }
}

/** ******************************************************** */
/**            Sending data to the GUI/server                */
/** ******************************************************** */

void send_join(int sockfd, char *player_name) {
    str_len_t name_len = (str_len_t) player_name[0];

    buffer_t *buffer = buffer_new();

    msg_type_t msg_type = JOIN;
    buffer_push(buffer, &msg_type, sizeof msg_type);
    buffer_push(buffer, player_name, sizeof name_len + name_len);

    send(sockfd, buffer->buf, buffer->size, 0);

    buffer_free(buffer);
}

void send_input(int sockfd, struct msg_input input) {
    buffer_t *buffer = buffer_new();

    msg_type_t msg_type;
    if (input.type == GUI_PLACE_BOMB)
        msg_type = SRV_PLACE_BOMB;
    else if (input.type == GUI_PLACE_BLOCK)
        msg_type = SRV_PLACE_BLOCK;
    else if (input.type == GUI_MOVE)
        msg_type = SRV_MOVE;

    buffer_push(buffer, &msg_type, sizeof msg_type);
    if (msg_type == SRV_MOVE)
        buffer_push(buffer, &input.direction, sizeof input.direction);

    send(sockfd, buffer->buf, buffer->size, 0);
    buffer_free(buffer);
}

void send_lobby(int sockfd, struct msg_hello hello, struct msg_player players[],
                uint32_t curr_players_count, struct addrinfo *gui_info) {
    buffer_t *buffer = buffer_new();

    // push message type
    msg_type_t msg_type = LOBBY;
    buffer_push(buffer, &msg_type, sizeof msg_type);

    // push server name
    str_len_t strlen = (str_len_t) hello.server_name[0];
    buffer_push(buffer, hello.server_name, strlen + 1);

    // push all the numeric data
    hello.bomb_timer = htons(hello.bomb_timer);
    hello.explosion_radius = htons(hello.explosion_radius);
    hello.size_x = htons(hello.size_x);
    hello.size_y = htons(hello.size_y);
    hello.game_length = htons(hello.game_length);
    buffer_push(buffer, (char *) &hello + sizeof(char *), sizeof hello - sizeof(char *));

    // push the players map
    map_len_t players_count = htonl(curr_players_count);
    buffer_push(buffer, &players_count, sizeof players_count);

    for (size_t i = 0; i < curr_players_count; i++)
        serialize_player(buffer, &players[i]);

    return send_to_gui(sockfd, buffer->buf, buffer->size, gui_info);

    buffer_free(buffer);
}

void send_game(int sockfd, struct game_state *state, struct msg_hello hello,
               struct msg_player players[], struct addrinfo *gui_info) {
    buffer_t *buffer = buffer_new();

    // push message type
    msg_type_t msg_type = GAME;
    buffer_push(buffer, &msg_type, sizeof msg_type);

    // push server name
    str_len_t strlen = (str_len_t) hello.server_name[0];
    buffer_push(buffer, hello.server_name, sizeof strlen + strlen);

    // push all the numeric data
    uint16_t size_x = htons(hello.size_x);
    buffer_push(buffer, &size_x, sizeof size_x);

    uint16_t size_y = htons(hello.size_y);
    buffer_push(buffer, &size_y, sizeof size_y);

    uint16_t game_length = htons(hello.game_length);
    buffer_push(buffer, &game_length, sizeof game_length);

    uint16_t turn = htons(state->turn);
    buffer_push(buffer, &turn, sizeof turn);

    // push the players map
    map_len_t players_count = htonl(hello.players_count);
    buffer_push(buffer, &players_count, sizeof players_count);

    for (int i = 0; i < hello.players_count; i++)
        serialize_player(buffer, &players[i]);

    // push the player_positions map
    buffer_push(buffer, &players_count, sizeof players_count);

    for (int i = 0; i < hello.players_count; i++) {
        uint8_t curr_id = players[i].id;
        buffer_push(buffer, &curr_id, sizeof curr_id);

        struct position pos = state->players[curr_id];
        buffer_push(buffer, &pos, sizeof pos);
    }

    buffer_t *temp = buffer_new();
    serialize_explosions(temp, state, hello.size_x, hello.size_y);
    serialize_blocks(buffer, state, hello.size_x, hello.size_y);
    serialize_bombs(buffer, state);
    buffer_push(buffer, temp->buf, temp->size);
    serialize_scores(buffer, state, players, hello.players_count);

    send_to_gui(sockfd, buffer->buf, buffer->size, gui_info);

    buffer_free(temp);
    buffer_free(buffer);
}
