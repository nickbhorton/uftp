#include "uftp2.h"
#include "debug_macros.h"

#include <poll.h>

int send_packet(
    int sockfd,
    const Address* to,
    const PacketHeaderSend head,
    const StringView payload
)
{
    static struct pollfd pfd[1];
    pfd[0].fd = sockfd;
    pfd[0].events = POLLOUT;

    PacketHeader head_n = {
        .packet_length = htonl(sizeof(PacketHeader) + payload.len),
        .version = htonl(UFTP_PROTOCOL_VERSION),
        .sequence_number = htonl(head.sequence_number),
        .sequence_total = htonl(head.sequence_total),
        .function = htonl(head.function)
    };

    unsigned int total_bytes_sent = 0;
    unsigned int bytes_to_send = payload.len + sizeof(PacketHeader);
    int rv = 0;
    while (total_bytes_sent < bytes_to_send) {
        int number_events = poll(pfd, 1, UFTP_TIMEOUT_MS);
        if (number_events < 0) {
            int en = errno;
            UFTP_DEBUG_ERR("send_packet::poll() errno %i\n", en);
            return -1;
        } else if (number_events == 0) {
            UFTP_DEBUG_MSG("send_packet::poll() timeout\n");
            return total_bytes_sent;
        } else if (!(pfd[0].revents & POLLOUT)) {
            UFTP_DEBUG_ERR("send_packet::poll() returned unwanted event\n");
            return -1;
        }
        if (total_bytes_sent < sizeof(PacketHeader)) {
            rv = sendto(
                pfd[0].fd,
                ((char*)&head_n) + total_bytes_sent,
                sizeof(PacketHeader) - total_bytes_sent,
                0,
                Address_sockaddr(to),
                to->addrlen
            );
        } else {
            rv = sendto(
                pfd[0].fd,
                payload.data + (total_bytes_sent - sizeof(PacketHeader)),
                payload.len - (total_bytes_sent - sizeof(PacketHeader)),
                0,
                Address_sockaddr(to),
                to->addrlen
            );
        }
        if (rv < 0) {
            int en = errno;
            UFTP_DEBUG_ERR("send_packet::sendto() errno %i\n", en);
            return -1;
        } else if (rv == 0) {
            UFTP_DEBUG_MSG("send_packet::sendto() returned 0\n");
        }
        total_bytes_sent += rv;
    }
    return total_bytes_sent;
}

int recv_packet(
    int sockfd,
    Address* from,
    PacketHeader* header_o,
    char* payload_buffer_o
)
{
    static char buffer[UFTP_BUFFER_SIZE];
    static bool buffer_init = false;
    if (!buffer_init) {
        memset(buffer, 0, UFTP_BUFFER_SIZE);
        buffer_init = true;
    }

    static struct pollfd pfd[1];
    pfd[0].fd = sockfd;
    pfd[0].events = POLLIN;

    unsigned int total_bytes_recv = 0;
    int rv = 0;

    bool parsed_packet_length = false;
    while (1) {
        int number_events = poll(pfd, 1, UFTP_TIMEOUT_MS);
        if (number_events < 0) {
            int en = errno;
            UFTP_DEBUG_ERR("recv_packet::poll() errno %i\n", en);
            return -1;
        } else if (number_events == 0) {
            UFTP_DEBUG_MSG("recv_packet::poll() timeout\n");
            return total_bytes_recv;
        } else if (!(pfd[0].revents & POLLIN)) {
            UFTP_DEBUG_ERR("recv_packet::poll() returned unwanted event\n");
            return -1;
        }
        from->addrlen = sizeof(from->addr);
        rv = recvfrom(
            sockfd,
            buffer + total_bytes_recv,
            UFTP_BUFFER_SIZE - total_bytes_recv,
            0,
            Address_sockaddr(from),
            &from->addrlen
        );
        if (rv < 0) {
            int en = errno;
            UFTP_DEBUG_ERR("recv_packet::recvfrom() errno %i\n", en);
            return -1;
        } else if (rv == 0) {
            UFTP_DEBUG_MSG("recv_packet::recvfrom() returned 0\n");
        }
        total_bytes_recv += rv;
        if (!parsed_packet_length && total_bytes_recv >= sizeof(uint32_t)) {
            header_o->packet_length = ntohl(*((uint32_t*)buffer));
            if (header_o->packet_length > UFTP_BUFFER_SIZE) {
                UFTP_DEBUG_ERR("recv_packet() packet larger than buffer\n");
                return -1;
            }
            parsed_packet_length = true;
        }
        // exit condition
        if (parsed_packet_length &&
            total_bytes_recv == header_o->packet_length) {
            break;
        }
    }
    if (total_bytes_recv >= sizeof(PacketHeader)) {
        header_o->version =
            ntohl(*((uint32_t*)(buffer + (1 * sizeof(uint32_t)))));
        header_o->sequence_number =
            ntohl(*((uint32_t*)(buffer + (2 * sizeof(uint32_t)))));
        header_o->sequence_total =
            ntohl(*((uint32_t*)(buffer + (3 * sizeof(uint32_t)))));
        header_o->function =
            ntohl(*((uint32_t*)(buffer + (4 * sizeof(uint32_t)))));
    } else {
        UFTP_DEBUG_MSG("recv_packet() not enough bytes to parse header\n");
    }
    if (total_bytes_recv > sizeof(PacketHeader)) {
        memcpy(
            payload_buffer_o,
            buffer + sizeof(PacketHeader),
            header_o->packet_length - sizeof(PacketHeader)
        );
    }
    if (header_o->version != UFTP_PROTOCOL_VERSION) {
        UFTP_DEBUG_ERR(
            "recv_packet() versions don't match %i vs %i\n",
            header_o->version,
            UFTP_PROTOCOL_VERSION
        );
        return -1;
    }
    memset(buffer, 0, total_bytes_recv);
    return total_bytes_recv;
}

int get_address(const char* address, const char* port, Address* server_address)
{
    struct addrinfo serv_hints, *serv_res;
    memset(&serv_hints, 0, sizeof(serv_hints));
    serv_hints.ai_family = AF_INET;
    serv_hints.ai_socktype = SOCK_DGRAM;

    int ret;
    ret = getaddrinfo(address, port, &serv_hints, &serv_res);

    if (ret != 0) {
        UFTP_DEBUG_ERR(
            "get_address::getaddrinfo() error: %s\n",
            gai_strerror(ret)
        );
        freeaddrinfo(serv_res);
        return -1;
    }
    if (serv_res == NULL) {
        UFTP_DEBUG_ERR(
            "get_address() address for %s:%s was NULL\n",
            address,
            port
        );
        freeaddrinfo(serv_res);
        return -1;
    }

    server_address->addrlen = serv_res->ai_addrlen;
    memcpy(&server_address->addr, serv_res->ai_addr, server_address->addrlen);

    freeaddrinfo(serv_res);
    return 0;
}

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
    int rv;
    rv = getaddrinfo(addr, port, &hints, &address_info);
    if (rv != 0) {
        UFTP_DEBUG_ERR(
            "get_udp_socket::getaddrinfo() error: %s\n",
            gai_strerror(rv)
        );
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
            int en = errno;
            UFTP_DEBUG_ERR(
                "get_udp_socket::socket() error: %s\n",
                strerror(en)
            );
            continue; // next loop
        }
        if ((bind(bound_sock.fd, ptr->ai_addr, ptr->ai_addrlen)) < 0) {
            // put current address into bound_sock
            memcpy(&bound_sock.address.addr, ptr->ai_addr, ptr->ai_addrlen);
            bound_sock.address.addrlen = ptr->ai_addrlen;

            int en = errno;
            close(bound_sock.fd);
            char failed_addr[INET6_ADDRSTRLEN];
            memset(&failed_addr, 0, sizeof(failed_addr));
            inet_ntop(
                ptr->ai_family,
                Address_in_addr(&bound_sock.address),
                failed_addr,
                sizeof(failed_addr)
            );
            UFTP_DEBUG_ERR(
                "get_udp_socket() failed to bind() to %s: %s\n",
                strerror(en),
                failed_addr
            );
            continue; // next loop
        }
        break; // if here we have a valid bound SOCK_DGRAM socket
    }

    if (ptr == NULL) {
        UFTP_DEBUG_ERR("get_udp_socket() failed to find and bind a socket\n");
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

    freeaddrinfo(address_info);

    // setting sockopts
    int yes = 1;
    if (setsockopt(bound_sock.fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) <
        0) {
        int en = errno;
        UFTP_DEBUG_ERR("get_udp_socket::setsockopt() %s\n", strerror(en));
        close(bound_sock.fd);

        // error value goes into sockfd of bound_sock
        memset(&bound_sock, 0, sizeof(bound_sock));
        bound_sock.fd = -1;
        return bound_sock;
    }

    return bound_sock;
}

/*
 * from beej returns a pointer to either a addr_sin or addr_sin6 dependand on
 * addr.ss_family
 */
void* Address_in_addr(const Address* a)
{
    if (a->addr.ss_family == AF_INET) {
        return &(((struct sockaddr_in*)&a->addr)->sin_addr);
    }

    return &(((struct sockaddr_in6*)&a->addr)->sin6_addr);
}

struct sockaddr* Address_sockaddr(const Address* a)
{
    return (struct sockaddr*)&a->addr;
}

uint16_t Address_port(const Address* a)
{
    if (a->addr.ss_family == AF_INET) {
        return ntohs(((struct sockaddr_in*)&a->addr)->sin_port);
    } else if (a->addr.ss_family == AF_INET6) {
        return ntohs(((struct sockaddr_in6*)&a->addr)->sin6_port);
    }
    return 0;
}

/*
 * Potentailly very harmfull command, should not be used based on any user
 * input, the cmd parameter should be directly filled with a c string constant.
 * returns:
 *   -1 if popen() fails
 *   >=0 if no error and indicates number of bytes in cout_o
 */
int get_shell_cmd_cout(const char* cmd, String* cout_o)
{
    FILE* fptr;
    fptr = popen(cmd, "r");
    if (fptr == NULL) {
        fprintf(stderr, "get_shell_cmd_cout() error: popen() failed\n");
        return -1;
    }

    int c = 0;
    size_t char_count = 0;
    while ((c = fgetc(fptr)) != EOF) {
        String_push_back(cout_o, (char)c);
        char_count++;
    }
    return char_count;
}
