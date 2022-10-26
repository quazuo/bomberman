#include "net.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <stdint.h>

#include "utils/err.h"

uint16_t parse_port(char *string) {
    errno = 0;
    unsigned long port = strtoul(string, NULL, 10);
    PRINT_ERRNO();
    if (port > UINT16_MAX)
        fatal("%s is not a valid port number", string);

    return (uint16_t) port;
}

struct addrinfo *parse_addr(char *addr, struct addrinfo *hints) {
    struct addrinfo *res;

    int last_colon = -1;
    for (int i = 0; addr[i] != '\0'; i++) {
        if (addr[i] == ':')
            last_colon = i;
    }

    ENSURE(last_colon != -1);

    addr[last_colon] = '\0';
    char *port = addr + last_colon + 1;

    int retval = getaddrinfo(addr, port, hints, &res);
    if (retval != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retval));
        return NULL;
    }

    return res;
}

int bind_socket_udp(uint16_t port) {
    int socket_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    ENSURE(socket_fd >= 0);

    struct sockaddr_in6 server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin6_family = AF_INET6;
    server_address.sin6_addr = in6addr_any;
    server_address.sin6_port = htons(port);

    CHECK_ERRNO(bind(socket_fd, (struct sockaddr *) &server_address,
                     (socklen_t) sizeof(server_address)));

    return socket_fd;
}

void send_to_gui(int fd, void *buf, size_t n, struct addrinfo *gui_info) {
    sendto(fd, buf, n, 0, gui_info->ai_addr, gui_info->ai_addrlen);
}
