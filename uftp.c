#include "uftp.h"

int get_udp_socket(const char* addr, const char* port)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // ipv4
    hints.ai_socktype = SOCK_DGRAM;
    if (addr == NULL) {
        hints.ai_flags = AI_PASSIVE;
    }

    struct addrinfo* address_info;
    int ret;
    ret = getaddrinfo(addr, port, &hints, &address_info);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo() error: %s\n", gai_strerror(ret));
        return -1;
    }

    // linked list traversal vibe from beej.us
    int sfd;
    struct addrinfo* ptr;
    for (ptr = address_info; ptr != NULL; ptr = ptr->ai_next) {
        if ((sfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) <
            0) {
            int sock_err = errno;
            fprintf(stderr, "socket() error: %s\n", strerror(sock_err));
            continue; // next loop
        }
        if ((bind(sfd, ptr->ai_addr, ptr->ai_addrlen)) < 0) {
            int sock_err = errno;
            close(sfd);
            char failed_addr[INET6_ADDRSTRLEN];
            memset(&failed_addr, 0, sizeof(failed_addr));
            inet_ntop(
                ptr->ai_family,
                get_in_addr(ptr->ai_addr),
                failed_addr,
                sizeof(failed_addr)
            );
            fprintf(
                stderr,
                "failed to bind() to %s: %s\n",
                strerror(sock_err),
                failed_addr
            );
            continue; // next loop
        }
        break; // if here we have a valid bound SOCK_DGRAM socket
    }

    if (ptr == NULL) {
        fprintf(stderr, "failed to find and bind a socket");
        return -1;
    }

    char success_addr[INET6_ADDRSTRLEN];
    memset(&success_addr, 0, sizeof(success_addr));
    inet_ntop(
        ptr->ai_family,
        get_in_addr(ptr->ai_addr),
        success_addr,
        sizeof(success_addr)
    );
    fprintf(stdout, "bound to address %s\n", success_addr);

    freeaddrinfo(address_info);

    // setting sockopts
    int yes = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        int sock_err = errno;
        fprintf(stderr, "socket() error: %s\n", strerror(sock_err));
        close(sfd);
        return -1;
    }

    return sfd;
}

void* get_in_addr(struct sockaddr* sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
