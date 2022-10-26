#ifndef ROBOTS_MSG
#define ROBOTS_MSG

#include <stdint.h>
#include <stddef.h>
#include <netdb.h>

#define MAX_CLIENT_COUNT 25

// constants for `client -> gui` messages
#define LOBBY           0
#define GAME            1

// constants for `gui -> client` messages
#define GUI_PLACE_BOMB  0
#define GUI_PLACE_BLOCK 1
#define GUI_MOVE        2
#define GUI_ERR         255

// constants for `client -> server` messages
#define JOIN            0
#define SRV_PLACE_BOMB  1
#define SRV_PLACE_BLOCK 2
#define SRV_MOVE        3

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

struct list {
    size_t size;
    void *arr;
};

struct __attribute__((packed)) position {
    uint16_t x;
    uint16_t y;
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
};

struct __attribute__((packed)) event_bomb_placed {
    bomb_id_t bomb_id;
    struct position pos;
};

struct event_bomb_exploded {
    bomb_id_t bomb_id;
    struct list *robots_destroyed;
    struct list *blocks_destroyed;
};

struct __attribute__((packed)) event_player_moved {
    uint8_t player_id;
    struct position pos;
};

struct __attribute__((packed)) event_block_placed {
    struct position pos;
};

struct msg_event {
    msg_type_t event_type;
    union {
        struct event_bomb_placed bomb_placed;
        struct event_bomb_exploded bomb_exploded;
        struct event_player_moved player_moved;
        struct event_block_placed block_placed;
    } event_data;
};

struct msg_turn {
    uint16_t turn;
    struct list *event_list;
};

struct __attribute__((packed)) msg_score {
    player_id_t player_id;
    score_t score;
};

struct __attribute__((packed)) msg_input {
    uint8_t type;
    uint8_t direction;
};

struct msg_hello parse_hello(int sockfd);

struct msg_player parse_player(int sockfd);

void parse_game_started(int sockfd, struct msg_player players[]);

struct msg_turn parse_turn(int sockfd);

struct msg_score *parse_game_ended(int sockfd);

struct msg_input parse_input(int sockfd);

void send_input(int sockfd, struct msg_input input);

void send_join(int sockfd, char *player_name);

void send_lobby(int sockfd, struct msg_hello hello, struct msg_player players[],
                uint32_t curr_players_count, struct addrinfo *gui_info);

struct game_state;

void send_game(int sockfd, struct game_state *state, struct msg_hello hello,
               struct msg_player players[], struct addrinfo *gui_info);

#endif // ROBOTS_MSG
