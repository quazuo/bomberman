#include "args.h"

#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <getopt.h>

#include "net.h"
#include "msg.h"
#include "utils/err.h"

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

    DECLARE_HELP_ITEM("-d, --gui-address address:port",
                      "[Required] Connect to a GUI at the specified address and port.");

    DECLARE_HELP_ITEM("-s, --server-address address:port",
                      "[Required] Connect to a game server at the specified address and port.");

    DECLARE_HELP_ITEM("-n, --player-name",
                      "[Required] Specify your player name.");

    DECLARE_HELP_ITEM("-p, --port",
                      "[Required] Specify the port for receiving messages from the GUI.");

    unsigned long max_width = 0;
    for (int i = 0; i < HELP_ITEM_COUNT; i += 2)
        max_width = strlen(HELP_ITEM(i)) > max_width ? strlen(HELP_ITEM(i)) : max_width;

    printf("Usage: %s [options]\n", prog_name);
    for (int i = 0; i < HELP_ITEM_COUNT; i += 2) {
        printf("%-*s %s\n", (int) max_width, HELP_ITEM(i), HELP_ITEM(i + 1));
    }
}

struct prog_args parse_args(int argc, char **argv) {
    struct prog_args args;
    memset(&args, 0, sizeof(args));

    struct option long_options[] = {
        {"gui-address",    required_argument, NULL,      'd'},
        {"help",           no_argument, &args.help_flag, 'h'},
        {"player-name",    required_argument, NULL,      'n'},
        {"port",           required_argument, NULL,      'p'},
        {"server-address", required_argument, NULL,      's'},
        {0, 0, 0, 0}
    };

    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "hd:n:p:s:", long_options, &option_index);

        if (c == -1)
            break;

        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));

        switch (c) {
            case 'd':
                hints.ai_socktype = SOCK_DGRAM;
                args.gui_out_info = parse_addr(optarg, &hints);
                break;

            case 'h':
                args.help_flag = 1;
                break;

            case 'n':;
                str_len_t name_len = (str_len_t) strlen(optarg);
                args.player_name = malloc((sizeof(name_len) + name_len) * sizeof(char));
                ENSURE(args.player_name != NULL);

                // serialize it immediately
                memcpy(args.player_name, &name_len, sizeof(name_len));
                memcpy(args.player_name + sizeof(name_len), optarg, name_len);
                break;

            case 'p':
                args.gui_in_port = parse_port(optarg);
                break;

            case 's':
                hints.ai_socktype = SOCK_STREAM;
                args.srv_info = parse_addr(optarg, &hints);
                break;

            default:
                fatal("getopt_long");
                break;
        }
    }

    return args;
}

void free_args(struct prog_args *args) {
    freeaddrinfo(args->gui_out_info);
    freeaddrinfo(args->srv_info);
    free(args->player_name);
}