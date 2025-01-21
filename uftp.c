#include "uftp.h"

UdpBoundSocket get_udp_socket(const char* addr, const char* port)
{
    UdpBoundSocket bound_sock;

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

        // error value goes into sockfd of bound_sock
        memset(&bound_sock, 0, sizeof(bound_sock));
        bound_sock.fd = -1;
        return bound_sock;
    }

    // linked list traversal vibe from beej.us
    struct addrinfo* ptr;
    for (ptr = address_info; ptr != NULL; ptr = ptr->ai_next) {
        if ((bound_sock.fd =
                 socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) <
            0) {
            int sock_err = errno;
            fprintf(stderr, "socket() error: %s\n", strerror(sock_err));
            continue; // next loop
        }
        if ((bind(bound_sock.fd, ptr->ai_addr, ptr->ai_addrlen)) < 0) {
            // put current address into bound_sock
            memcpy(&bound_sock.address.addr, ptr->ai_addr, ptr->ai_addrlen);
            bound_sock.address.addrlen = ptr->ai_addrlen;

            int sock_err = errno;
            close(bound_sock.fd);
            char failed_addr[INET6_ADDRSTRLEN];
            memset(&failed_addr, 0, sizeof(failed_addr));
            inet_ntop(
                ptr->ai_family,
                Address_in_addr(&bound_sock.address),
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

        // error value goes into sockfd of bound_sock
        memset(&bound_sock, 0, sizeof(bound_sock));
        bound_sock.fd = -1;
        return bound_sock;
    }

    // put success address into bound_sock
    memcpy(&bound_sock.address.addr, ptr->ai_addr, ptr->ai_addrlen);
    bound_sock.address.addrlen = ptr->ai_addrlen;

    char success_addr[INET6_ADDRSTRLEN];
    memset(&success_addr, 0, sizeof(success_addr));
    inet_ntop(
        bound_sock.address.addr.ss_family,
        Address_in_addr(&bound_sock.address),
        success_addr,
        sizeof(success_addr)
    );
    fprintf(stdout, "bound to address %s\n", success_addr);

    freeaddrinfo(address_info);

    // setting sockopts
    int yes = 1;
    if (setsockopt(bound_sock.fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) <
        0) {
        int sock_err = errno;
        fprintf(stderr, "socket() error: %s\n", strerror(sock_err));
        close(bound_sock.fd);

        // error value goes into sockfd of bound_sock
        memset(&bound_sock, 0, sizeof(bound_sock));
        bound_sock.fd = -1;
        return bound_sock;
    }

    return bound_sock;
}

void* Address_in_addr(Address* a)
{
    if (a->addr.ss_family == AF_INET) {
        return &(((struct sockaddr_in*)&a->addr)->sin_addr);
    }

    return &(((struct sockaddr_in6*)&a->addr)->sin6_addr);
}

struct sockaddr* Address_sockaddr(Address* a)
{
    return (struct sockaddr*)&a->addr;
}

uint16_t Address_port(Address* a)
{
    if (a->addr.ss_family == AF_INET) {
        return ntohs(((struct sockaddr_in*)&a->addr)->sin_port);
    } else if (a->addr.ss_family == AF_INET6) {
        return ntohs(((struct sockaddr_in6*)&a->addr)->sin6_port);
    }
    return 0;
}
