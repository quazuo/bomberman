#include "net.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <unistd.h>

#include "utils/err.h"
#include "msg.h"

uint16_t parse_port(char *string) {
    errno = 0;
    unsigned long port = strtoul(string, NULL, 10);
    PRINT_ERRNO();
    if (port > UINT16_MAX) {
        fatal("%s is not a valid port number", string);
    }

    return (uint16_t) port;
}

int bind_socket_tcp(uint16_t port) {
    int socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
    ENSURE(socket_fd >= 0);

    struct sockaddr_in6 server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin6_family = AF_INET6;
    server_address.sin6_addr = in6addr_any;
    server_address.sin6_port = htons(port);

    CHECK_ERRNO(bind(socket_fd, (struct sockaddr *) &server_address,
                     (socklen_t) sizeof server_address));

    return socket_fd;
}

void accept_client(int srvfd, int *fd, struct msg_player *player) {
    int client_fd = accept(srvfd, NULL, NULL);
    ENSURE(client_fd >= 0);
    *fd = client_fd;

    // set a 1-second timeout for receiving messages, so that we don't block infinitely
    // after receiving only a part of a message which we expect to contain more info.
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    CHECK(setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof tv));

    // turn off Nagle's algorithm
    int yes = 1;
    CHECK(setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (char *) &yes, sizeof(int)));

    // save the address
    struct sockaddr_in6 client_addr;
    getpeername(client_fd, (struct sockaddr *) &client_addr, (socklen_t *) sizeof client_addr);
    struct in6_addr ip = client_addr.sin6_addr;

    // serialize it immediately
    player->address = malloc((MAX_STR_LEN + 1) * sizeof(char));
    ENSURE(player->address != NULL);

    char temp[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &ip, temp, INET6_ADDRSTRLEN);

    str_len_t str_len = (str_len_t) sprintf(player->address + 1, "[%s]:%d", temp, client_addr.sin6_port);
    memcpy(player->address, &str_len, sizeof str_len);

    player->port = client_addr.sin6_port;
}

void disconnect_client(int *fd) {
    close(*fd);
    *fd = -1;
}

int recv_check(int *fd, void *buf, size_t n) {
    errno = 0;

    recv(*fd, buf, n, MSG_WAITALL);
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        disconnect_client(fd);
        return 1;
    }

    return 0;
}

void socket_flush(const int *fd) {
    ssize_t read_len;
    size_t bufsize = 32;
    char buffer[bufsize];

    do {
        read_len = recv(*fd, buffer, bufsize, MSG_DONTWAIT);
    } while (read_len > 0);
}
