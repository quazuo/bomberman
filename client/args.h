#ifndef ROBOTS_CLIENT_ARGS_H
#define ROBOTS_CLIENT_ARGS_H

#include <stdint.h>

struct prog_args {
    struct addrinfo *gui_out_info;
    struct addrinfo *srv_info;
    char *player_name;
    uint16_t gui_in_port;
    int help_flag;
};

void print_help_info(char *prog_name);

struct prog_args parse_args(int argc, char **argv);

void free_args(struct prog_args *args);

#endif //ROBOTS_CLIENT_ARGS_H
