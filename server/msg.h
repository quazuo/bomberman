#ifndef ROBOTS_MSG
#define ROBOTS_MSG

#include <stdint.h>
#include <stddef.h>

#include "utils/buffer.h"
#include "args.h"

#define MAX_STR_LEN     255

// constants for `client -> server` messages
#define NONE            0
#define JOIN            0
#define PLACE_BOMB      1
#define PLACE_BLOCK     2
#define MOVE            3
#define ERR             255

// constants for `server -> client` messages
#define HELLO           0
#define ACCEPTED_PLAYER 1
#define GAME_STARTED    2
#define TURN            3
#define GAME_ENDED      4

// constants for event types
#define BOMB_PLACED     0
#define BOMB_EXPLODED   1
#define PLAYER_MOVED    2
#define BLOCK_PLACED    3

typedef uint8_t msg_type_t;
typedef uint8_t str_len_t;
typedef uint32_t list_len_t;
typedef uint32_t map_len_t;

typedef uint8_t player_id_t;
typedef uint32_t bomb_id_t;
typedef uint32_t score_t;

struct __attribute__((packed)) position {
    uint16_t x;
    uint16_t y;
};

struct __attribute__((packed)) msg_action {
    uint8_t type;
    uint8_t direction;
};

struct __attribute__((packed)) msg_hello {
    char *server_name;
    uint8_t players_count;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t game_length;
    uint16_t explosion_radius;
    uint16_t bomb_timer;
};

struct msg_player {
    player_id_t id;
    char *name;
    char *address;
    uint16_t port;
};

struct __attribute__((packed)) msg_score {
    player_id_t player_id;
    score_t score;
};

/** ******************************************************** */
/**                    Deserialization                       */
/** ******************************************************** */

char *parse_string(int *sockfd);

struct msg_action parse_action(int *sockfd, msg_type_t msg_type);

/** ******************************************************** */
/**                      Serialization                       */
/** ******************************************************** */

void serialize_hello(buffer_t *buffer, struct msg_hello hello);

/** ******************************************************** */
/**                      Communication                       */
/** ******************************************************** */

struct msg_hello build_hello(struct prog_args args);

void send_hello(int fd, buffer_t *hello_buf);

void send_accepted_player(int fd, struct msg_player *player);

void send_game_started(int fd, struct msg_player *players, uint8_t players_count);

void send_turn(int fd, buffer_t *turn_info, uint16_t turn);

void send_turns_recap(int fd, buffer_t **turns, uint16_t turn);

buffer_t *build_game_ended(score_t scores[], uint8_t players_count);

#endif // ROBOTS_MSG
