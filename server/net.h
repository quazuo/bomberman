#ifndef ROBOTS_NET_UTILS
#define ROBOTS_NET_UTILS

#include <netdb.h>

#include "utils/buffer.h"

#define MAX_CLIENT_COUNT    25
#define QUEUE_LEN           5
#define N_FDS               MAX_CLIENT_COUNT + 1

uint16_t parse_port(char *string);

int bind_socket_tcp(uint16_t port);

struct msg_player; // forward declaration for `accept_client()`
void accept_client(int srvfd, int *fd, struct msg_player *player);

void disconnect_client(int *fd);

int recv_check(int *fd, void *buf, size_t n);

void socket_flush(const int *fd);

#endif // ROBOTS_NET_UTILS
