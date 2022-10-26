#include "msg.h"

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "utils/err.h"
#include "net.h"
#include "utils/buffer.h"
#include "utils/hmap.h"

/** ******************************************************** */
/**                    Deserialization                       */
/** ******************************************************** */

char *parse_string(int *sockfd) {
    str_len_t len;
    recv_check(sockfd, &len, sizeof len);

    char *str = malloc((sizeof len + len) * sizeof *str);
    ENSURE(str != NULL);
    memcpy(str, &len, sizeof len);
    recv_check(sockfd, str + sizeof len, len);

    return str;
}

struct msg_action parse_action(int *sockfd, msg_type_t action_type) {
    struct msg_action action;

    action.type = action_type;

    if (action.type == MOVE)
        recv_check(sockfd, (char *) &action.direction, sizeof action.direction);

    return action;
}

/** ******************************************************** */
/**                      Serialization                       */
/** ******************************************************** */

void serialize_hello(buffer_t *buffer, struct msg_hello hello) {
    str_len_t name_len;
    memcpy(&name_len, hello.server_name, sizeof name_len);
    buffer_push(buffer, hello.server_name, name_len + sizeof name_len);
    buffer_push(buffer, (char *) &hello + sizeof(char *), sizeof hello - sizeof(char *));
}

void serialize_player(buffer_t *buffer, struct msg_player *player) {
    buffer_push(buffer, &player->id, sizeof player->id);

    str_len_t strlen = (str_len_t) player->name[0];
    buffer_push(buffer, player->name, strlen + 1);

    strlen = (str_len_t) player->address[0];
    buffer_push(buffer, player->address, strlen + 1);
}

/** ******************************************************** */
/**               Sending data to the client                 */
/** ******************************************************** */

struct msg_hello build_hello(struct prog_args args) {
    struct msg_hello hello;

    hello.server_name = args.server_name;
    hello.players_count = args.players_count;
    hello.size_x = htons(args.size_x);
    hello.size_y = htons(args.size_y);
    hello.game_length = htons(args.game_length);
    hello.explosion_radius = htons(args.explosion_radius);
    hello.bomb_timer = htons(args.bomb_timer);

    return hello;
}

void send_hello(int fd, buffer_t *hello_buf) {
    buffer_t *buffer = buffer_new();

    msg_type_t msg_type = HELLO;
    buffer_push(buffer, &msg_type, sizeof msg_type);
    buffer_push(buffer, hello_buf->buf, hello_buf->size);

    send(fd, buffer->buf, buffer->size, 0);

    buffer_free(buffer);
}

void send_accepted_player(int fd, struct msg_player *player) {
    buffer_t *buffer = buffer_new();

    msg_type_t msg_type = ACCEPTED_PLAYER;
    buffer_push(buffer, &msg_type, sizeof msg_type);
    serialize_player(buffer, player);

    send(fd, buffer->buf, buffer->size, 0);

    buffer_free(buffer);
}

void send_game_started(int fd, struct msg_player *players, uint8_t players_count) {
    buffer_t *buffer = buffer_new();

    msg_type_t msg_type = GAME_STARTED;
    buffer_push(buffer, &msg_type, sizeof msg_type);

    map_len_t map_len = htonl(players_count);
    buffer_push(buffer, &map_len, sizeof map_len);

    for (int i = 1; i < N_FDS; i++) {
        if (players[i].name)
            serialize_player(buffer, &players[i]);
    }

    send(fd, buffer->buf, buffer->size, 0);

    buffer_free(buffer);
}

void send_turn(int fd, buffer_t *turn_info, uint16_t turn) {
    buffer_t *buffer = buffer_new();

    msg_type_t msg_type = TURN;
    buffer_push(buffer, &msg_type, sizeof msg_type);

    uint16_t net_turn = htons(turn);
    buffer_push(buffer, &net_turn, sizeof net_turn);

    buffer_push(buffer, turn_info->buf, turn_info->size);

    send(fd, buffer->buf, buffer->size, 0);

    buffer_free(buffer);
}

void send_turns_recap(int fd, buffer_t **turns, uint16_t turn) {
    for (uint16_t i = 0; i < turn; i++)
        send_turn(fd, turns[i], i);
}

buffer_t *build_game_ended(score_t scores[], uint8_t players_count) {
    buffer_t *buffer = buffer_new();

    msg_type_t msg_type = GAME_ENDED;
    map_len_t map_len = htonl(players_count);

    buffer_push(buffer, &msg_type, sizeof msg_type);
    buffer_push(buffer, &map_len, sizeof map_len);

    for (player_id_t id = 0; id < players_count; id++) {
        buffer_push(buffer, &id, sizeof id);

        score_t net_score = htonl(scores[id]);
        buffer_push(buffer, &net_score, sizeof net_score);
    }

    return buffer;
}
