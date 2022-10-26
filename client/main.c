#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>

#include "net.h"
#include "utils/err.h"
#include "msg.h"
#include "game.h"
#include "args.h"

static int state = LOBBY; // == `LOBBY` or `GAME`

int main(int argc, char **argv) {
    struct prog_args args = parse_args(argc, argv);

    if (args.help_flag) {
        free_args(&args);
        print_help_info(argv[0]);
        return 0;
    }

    if (!args.gui_out_info || !args.srv_info || !args.player_name || !args.gui_in_port) {
        free_args(&args);
        fatal("some required arguments missing");
    }

    // socket fd for listening to data from the GUI server
    int gui_in_fd = bind_socket_udp(args.gui_in_port);

    // socket fd for sending data to the GUI server
    int gui_out_fd = socket(args.gui_out_info->ai_family, args.gui_out_info->ai_socktype, 0);
    ENSURE(gui_out_fd >= 0);
    //CHECK_ERRNO(connect(gui_out_fd, args.gui_out_info->ai_addr, args.gui_out_info->ai_addrlen));

    // socket fd for communicating with the server
    int srv_fd = socket(args.srv_info->ai_family, args.srv_info->ai_socktype, 0);
    ENSURE(srv_fd >= 0);
    CHECK_ERRNO(connect(srv_fd, args.srv_info->ai_addr, args.srv_info->ai_addrlen));
    int yes = 1;
    CHECK(setsockopt(srv_fd, IPPROTO_TCP, TCP_NODELAY, (char *) &yes, sizeof(int)));

    // read the initial 'Hello' message
    msg_type_t msg_type;
    recv(srv_fd, &msg_type, sizeof(msg_type), MSG_WAITALL);
    ENSURE(msg_type == HELLO);
    struct msg_hello hello = parse_hello(srv_fd);

    uint32_t curr_players_count = 0;
    struct msg_player players[MAX_CLIENT_COUNT];
    memset(players, 0, sizeof(players));

    struct game_state *game_state = init_state(&hello);

    send_lobby(gui_out_fd, hello, players, curr_players_count, args.gui_out_info);

    // `poll()` setup
    struct pollfd fds[2];

    for (int i = 0; i < 2; i++) {
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }

    fds[0].fd = gui_in_fd;
    fds[1].fd = srv_fd;

    while (true) {
        poll(fds, 2, -1);

        if (fds[0].revents & POLLIN) { // message from the GUI
            fds[0].revents = 0;

            struct msg_input input = parse_input(gui_in_fd);

            if (input.type != GUI_ERR) {
                if (state == LOBBY) {
                    send_join(srv_fd, args.player_name);

                } else { // state == GAME
                    send_input(srv_fd, input);
                }
            }

        } else if (fds[0].revents & (POLLERR | POLLHUP)) {
            printf("revents: %d\n", fds[0].revents);
            fprintf(stderr, "ERROR: connection with GUI lost\n");
            break;
        }

        if (fds[1].revents & POLLIN) { // message from the server
            fds[1].revents = 0;

            recv(srv_fd, (char *) &msg_type, sizeof(msg_type), MSG_WAITALL);

            if (msg_type == ACCEPTED_PLAYER) {
                ENSURE(state == LOBBY);
                struct msg_player player = parse_player(srv_fd);
                players[player.id] = player;
                curr_players_count++;

                send_lobby(gui_out_fd, hello, players, curr_players_count, args.gui_out_info);

            } else if (msg_type == GAME_STARTED) {
                ENSURE(state == LOBBY);
                parse_game_started(srv_fd, players);
                state = GAME;

            } else if (msg_type == TURN) {
                ENSURE(state == GAME);
                struct msg_turn turn = parse_turn(srv_fd);
                analyze_turn(game_state, &turn);

                send_game(gui_out_fd, game_state, hello, players, args.gui_out_info);

            } else if (msg_type == GAME_ENDED) {
                ENSURE(state == GAME);
                struct msg_score *scores = parse_game_ended(srv_fd);
                free(scores); // so far it's unused, but it might be used in the future

                for (uint32_t i = 0; i < curr_players_count; i++) {
                    free(players[i].name);
                    free(players[i].address);
                }
                memset(players, 0, sizeof(players));
                curr_players_count = 0;
                reset_state(game_state, &hello);

                send_lobby(gui_out_fd, hello, players, curr_players_count, args.gui_out_info);

                state = LOBBY;

            } else {
                fprintf(stderr, "ERROR: unrecognized message from server\n");
                break;
            }

        } else if (fds[1].revents & (POLLERR | POLLHUP)) {
            printf("revents: %d\n", fds[1].revents);
            fprintf(stderr, "ERROR: connection with server lost\n");
            break;
        }
    }

    free_args(&args);
    free_state(game_state, hello.size_x);
    free(hello.server_name);

    for (uint32_t i = 0; i < curr_players_count; i++) {
        free(players[i].name);
        free(players[i].address);
    }

    exit(1);
}
