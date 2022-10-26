#include "args.h"

#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "utils/err.h"
#include "net.h"
#include "msg.h"

// macros for `print_help_info()`
#define START_HELP_DECLS            \
    static int argc__ = 0;          \
    static char *info__[256]        \

#define DECLARE_HELP_ITEM(x, y)     \
    do {                            \
        info__[argc__++] = (x);     \
        info__[argc__++] = (y);     \
    } while (0)                     \

#define HELP_ITEM_COUNT argc__

#define HELP_ITEM(x) info__[(x)]

void print_help_info(char *prog_name) {
    START_HELP_DECLS;

    DECLARE_HELP_ITEM("-b, --bomb-timer <time>",
                      "[Required] The number of turns it takes for a bomb to explode.");

    DECLARE_HELP_ITEM("-c, --players-count <count>",
                      "[Required] Minimum number of players needed for a game to start.");

    DECLARE_HELP_ITEM("-d, --turn-duration <ms>",
                      "[Required] Amount of time a turn takes, in milliseconds.");

    DECLARE_HELP_ITEM("-e, --explosion-radius <radius>",
                      "[Required] Number of fields an explosion can reach at maximum.");

    DECLARE_HELP_ITEM("-k, --initial-blocks <count>",
                      "[Required] Number of blocks to be placed at the start of a game.");

    DECLARE_HELP_ITEM("-l, --game-length <length>",
                      "[Required] Specify how many turns should happen during each game.");

    DECLARE_HELP_ITEM("-n, --server-name <name>",
                      "[Required] The server's name.");

    DECLARE_HELP_ITEM("-p --port <port>",
                      "[Required] The port for accepting connections with clients.");

    DECLARE_HELP_ITEM("-x, --size-x <size>",
                      "[Required] Width of the game board.");

    DECLARE_HELP_ITEM("-y, --size-y <size>",
                      "[Required] Height of the game board.");

    DECLARE_HELP_ITEM("-s, --seed <seed>",
                      "A seed for predefining random behaviors, such as initial game board generation.");

    unsigned long max_first_width = 0;
    for (int i = 0; i < HELP_ITEM_COUNT; i += 2)
        max_first_width = strlen(HELP_ITEM(i)) > max_first_width ? strlen(HELP_ITEM(i)) : max_first_width;

    printf("Usage: %s [options]\n", prog_name);
    for (int i = 0; i < HELP_ITEM_COUNT; i += 2) {
        printf("%-*s %s\n", (int) max_first_width, HELP_ITEM(i), HELP_ITEM(i + 1));
    }
}

bool str_to_num__(char *str, void *dest, size_t size, uint64_t max) {
    errno = 0;
    char *endptr;

    unsigned long long val = strtoull(str, &endptr, 10);
    if (errno || *endptr != '\0' || val > max)
        return false;

    memcpy(dest, &val, size);
    return true;
}

void free_args(struct prog_args args) {
    free(args.server_name);
}

struct prog_args parse_args(int argc, char **argv) {
    struct prog_args args;
    memset(&args, 0, sizeof(args));

    struct option long_options[] = {
        {"help",             no_argument, &args.help_flag, 'h'},
        {"bomb-timer",       required_argument, NULL,      'b'},
        {"players-count",    required_argument, NULL,      'c'},
        {"turn-duration",    required_argument, NULL,      'd'},
        {"explosion-radius", required_argument, NULL,      'e'},
        {"initial-blocks",   required_argument, NULL,      'k'},
        {"game-length",      required_argument, NULL,      'l'},
        {"server-name",      required_argument, NULL,      'n'},
        {"port",             required_argument, NULL,      'p'},
        {"seed",             required_argument, NULL,      's'},
        {"size-x",           required_argument, NULL,      'x'},
        {"size-y",           required_argument, NULL,      'y'},
        {0, 0,                            0,               0}
    };

    bool provided[26]; // 26 = alphabet size
    memset(provided, 0, sizeof(provided));

    while (true) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "hb:c:d:e:k:l:n:p:s:x:y:", long_options, &option_index);

        if (c == -1)
            break;

        if (c >= 'a' && c <= 'z')
            provided[c - 'a'] = true;

        switch (c) {
            case 'h':
                args.help_flag = 1;
                break;

            case 'b':
                if (!str_to_num(optarg, &args.bomb_timer, UINT16_MAX))
                    fatal("Invalid arg: bomb-timer");
                break;

            case 'c':
                if (!str_to_num(optarg, &args.players_count, UINT8_MAX))
                    fatal("Invalid arg: players-count");
                break;

            case 'd':
                if (!str_to_num(optarg, &args.turn_duration, UINT64_MAX))
                    fatal("Invalid arg: turn-duration");
                break;

            case 'e':
                if (!str_to_num(optarg, &args.explosion_radius, UINT16_MAX))
                    fatal("Invalid arg: explosion-radius");
                break;

            case 'k':
                if (!str_to_num(optarg, &args.initial_blocks, UINT16_MAX))
                    fatal("Invalid arg: initial-blocks");
                break;

            case 'l':
                if (!str_to_num(optarg, &args.game_length, UINT16_MAX))
                    fatal("Invalid arg: game-length");
                break;

            case 'n':;
                str_len_t name_len = (str_len_t) strlen(optarg);
                args.server_name = malloc((sizeof(name_len) + name_len) * sizeof(char));
                ENSURE(args.server_name != NULL);

                // serialize it immediately
                memcpy(args.server_name, &name_len, sizeof(name_len));
                memcpy(args.server_name + sizeof(name_len), optarg, name_len);
                break;

            case 'p':
                args.port = parse_port(optarg);
                break;

            case 's':
                if (!str_to_num(optarg, &args.seed, UINT32_MAX))
                    fatal("Invalid arg: seed");
                args.provided_seed = true;
                break;

            case 'x':
                if (!str_to_num(optarg, &args.size_x, UINT16_MAX))
                    fatal("Invalid arg: size-x");
                break;

            case 'y':
                if (!str_to_num(optarg, &args.size_y, UINT16_MAX))
                    fatal("Invalid arg: size-y");
                break;

            default:
                fatal("getopt_long");
                break;
        }
    }

    if (!args.help_flag) {
        for (int i = 0; long_options[i].name; i++) {
            if (long_options[i].val != 'h'
                && long_options[i].val != 's'
                && !provided[long_options[i].val - 'a']) {
                free_args(args);
                fatal("missing argument: %s", long_options[i].name);
            }
        }
    }

    return args;
}
