#ifndef ROBOTS_NET_UTILS
#define ROBOTS_NET_UTILS

#include <netdb.h>

uint16_t parse_port(char *string);

struct addrinfo *parse_addr(char *addr, struct addrinfo *hints);

int bind_socket_udp(uint16_t port);

void send_to_gui(int fd, void *buf, size_t n, struct addrinfo *gui_info);

#endif // ROBOTS_NET_UTILS
