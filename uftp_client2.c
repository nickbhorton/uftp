#include "debug_macros.h"
#include "uftp2.h"
#include <errno.h>
#include <poll.h>

void useage();
int validate_address(int argc, char** argv);

void client_loop(int sockfd, Address* server_address)
{
    char buffer[UFTP_BUFFER_SIZE];
    memset(buffer, 0, UFTP_BUFFER_SIZE);
    PacketHeader head = {};

    struct pollfd pfds[2];
    pfds[0].events = POLLIN;
    pfds[0].fd = 0;
    pfds[1].events = POLLIN;
    pfds[1].fd = sockfd;
    int event_count = 0;
    int rv = 0;
    while ((event_count = poll(pfds, 2, UFTP_TIMEOUT_MS)) >= 0) {
        if (event_count == 0) {
            continue;
        }
        if (pfds[0].revents & POLLIN) {
            rv = read(pfds[0].fd, buffer, UFTP_BUFFER_SIZE);
            if (rv < 0) {
                int en = errno;
                UFTP_DEBUG_ERR("read returned %i errno:%i\n", rv, en);
                continue;
            }
            if (strncmp("exit\n", buffer, 5) == 0) {
                send_func_only(sockfd, server_address, CLIE_EXT);
            } else if (strncmp("ls\n", buffer, 3) == 0) {
                send_func_only(sockfd, server_address, CLIE_LS);
            }
            memset(buffer, 0, rv);
        }
        if (pfds[1].revents & POLLIN) {
            rv = recv_packet(sockfd, server_address, &head, buffer);
            if (rv < 0) {
                continue;
            }
            printf("packet length %i\n", head.packet_length);
            printf("packet function %i\n", head.function);
            for (size_t i = 0; i < head.packet_length - 20; i++) {
                printf("%c", buffer[i]);
            }
            memset(buffer, 0, rv);
            memset(&head, 0, sizeof(PacketHeader));
        }
    }
}

int main(int argc, char** argv)
{
    int rv = validate_address(argc, argv);
    UdpBoundSocket bs = get_udp_socket(NULL, "0");
    if (bs.fd < 0) {
        return 1;
    }
    Address server_address = {};
    rv = get_address(argv[1], argv[2], &server_address);
    if (rv < 0) {
        return 1;
    }

    client_loop(bs.fd, &server_address);
    close(bs.fd);
}
int validate_address(int argc, char** argv)
{
    int ret;
    if (argc != 3) {
        useage();
        return -1;
    }

    // ensure vaules passed are corrent.
    // TODO: find another way to do this
    struct sockaddr_in sa;
    ret = inet_pton(AF_INET, argv[1], &(sa.sin_addr));
    if (ret == 0) {
        fprintf(stderr, "address given was not a valid network address\n");
        return -1;
    } else if (ret < 0) {
        int inet_pton_err = errno;
        fprintf(
            stderr,
            "address family is not supported, %s\n",
            strerror(inet_pton_err)
        );
        return -1;
    }

    unsigned short int port;
    if (sscanf(argv[2], "%hu", &port) != 1) {
        fprintf(stderr, "port given was not a valid integer\n");
        return -1;
    }
    if (port < 5000) {
        fprintf(stderr, "port given to small, give a port greater than 5000\n");
        return -1;
    }
    return 0;
}

void useage()
{
    fprintf(
        stderr,
        "usage: uftp_client address port\n  address: the uftp servers "
        "address\n  port:    the uftp servers port\n"
    );
}
