#include "uftp.h"
#include <stdint.h>

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

int send_packet(int sockfd, Address* to, StringView packet)
{
    int bytes_sent;
    if ((bytes_sent = sendto(
             sockfd,
             packet.data,
             packet.len,
             0,
             Address_sockaddr(to),
             to->addrlen
         )) < 0) {
        int sendto_err = errno;
        fprintf(stderr, "sendto() error: %s\n", strerror(sendto_err));
        return -1;
    } else if (bytes_sent != packet.len) {
        fprintf(
            stderr,
            "send_packet() error: %s\n",
            "bytes sent were less than packet length"
        );
        return -1;
    }
    return bytes_sent;
}

int recv_packet(int sockfd, Address* from, String* packet_o)
{
    static bool buff_init = false;
    static char buffer[UFTP_BUFFER_SIZE];
    if (!buff_init) {
        memset(buffer, 0, UFTP_BUFFER_SIZE);
        buff_init = true;
    }
    int bytes_recv = recvfrom(
        sockfd,
        buffer,
        sizeof(buffer),
        0,
        (struct sockaddr*)&from->addr,
        &from->addrlen
    );

    if (bytes_recv >= 0) {
        *packet_o = String_create(buffer, bytes_recv);
        memset(buffer, 0, bytes_recv);
        return bytes_recv;
    } else {
        int recvfrom_err = errno;
        fprintf(stderr, "recv_packet() error: %s\n", strerror(recvfrom_err));
        return -1;
    }
}

int send_sequenced_packet(
    int sockfd,
    Address* to,
    char header[3],
    uint32_t seq_number,
    uint32_t seq_total,
    StringView payload
)
{
    String packet = String_create(header, 3);

    String_push_u32(&packet, htonl(payload.len));
    String_push_u32(&packet, htonl(seq_number));
    String_push_u32(&packet, htonl(seq_total));
    String_push_sv(&packet, payload);

    int bytes_sent_or_error =
        send_packet(sockfd, to, StringView_create(&packet, 0, packet.len));
    String_free(&packet);
    return bytes_sent_or_error;
}

int parse_sequenced_packet(
    const String* packet,
    char header_o[UFTP_HEADER_SIZE],
    uint32_t* seq_number_o,
    uint32_t* seq_total_o,
    String* payload_o
)
{
    if (packet->len < UFTP_HEADER_SIZE + 3 * sizeof(uint32_t)) {
        fprintf(
            stderr,
            "parse_sequence_packet() error, packet length was not long enough "
            "for parsing\n"
        );
        return -1;
    }
    for (size_t i = 0; i < UFTP_HEADER_SIZE; i++) {
        header_o[i] = String_get(packet, i);
    }
    uint32_t payload_len = ntohl(String_parse_u32(packet, UFTP_HEADER_SIZE));
    if (packet->len < UFTP_HEADER_SIZE + 3 * sizeof(uint32_t) + payload_len) {
        fprintf(
            stderr,
            "parse_sequence_packet() error, packet length was not long enough "
            "for payload\n"
        );
        return -1;
    }
    *seq_number_o =
        ntohl(String_parse_u32(packet, UFTP_HEADER_SIZE + 1 * sizeof(uint32_t))
        );
    *seq_total_o =
        ntohl(String_parse_u32(packet, UFTP_HEADER_SIZE + 2 * sizeof(uint32_t))
        );
    *payload_o = String_create(
        packet->data + UFTP_HEADER_SIZE + 3 * sizeof(uint32_t),
        payload_len
    );
    return 0;
}
