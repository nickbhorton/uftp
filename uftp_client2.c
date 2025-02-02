#include "String.h"
#include "debug_macros.h"
#include "uftp2.h"
#include <errno.h>
#include <poll.h>
#include <signal.h>

void useage();
int validate_address(int argc, char** argv);
int set_signals();

UdpBoundSocket bs;

void client_loop(int sockfd, Address* server_address)
{
    // clears term effects
    printf("\033[0m");

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
    bool exit = false;
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
                exit = true;
            } else if (strncmp("ls\n", buffer, 3) == 0) {
                send_func_only(sockfd, server_address, CLIE_LS);
            } else if (strncmp("set ", buffer, 4) == 0) {
                StringView sv = {.data = buffer + 4, .len = rv - 5};
                send_seq(sockfd, server_address, CLIE_SET_FN, 1, 1, sv);
            } else if (strncmp("size\n", buffer, 5) == 0) {
                send_func_only(sockfd, server_address, CLIE_GET_FS);
            } else if (strncmp("clear\n", buffer, 6) == 0) {
                printf("\033[2J\033[1;1H");
                fflush(stdout);
            } else if (strncmp("fc\n", buffer, 6) == 0) {
                send_empty_seq(sockfd, server_address, CLIE_GET_FC, 0, 1);
            }
            memset(buffer, 0, rv);
        }
        if (pfds[1].revents & POLLIN) {
            rv = recv_packet(sockfd, server_address, &head, buffer);
            if (rv < 0) {
                continue;
            }
            if (head.function == SERV_SUC) {
                printf("\033[0;32mSUC ");
            }
            if (head.function == SERV_ERR) {
                printf("\033[0;31mERR ");
            }
            if (head.function == SERV_BADF) {
                printf("\033[0;31mBADF ");
            }
            printf(
                "%i/%i\033[0m, %lu\n",
                head.sequence_number,
                head.sequence_total,
                head.packet_length - sizeof(PacketHeader)
            );
            for (size_t i = 0; i < head.packet_length - 20; i++) {
                printf("%c", buffer[i]);
            }
            fflush(stdout);

            // reset buffers
            memset(buffer, 0, rv);
            memset(&head, 0, sizeof(PacketHeader));

            if (exit) {
                break;
            }
        }
    }
}

int main(int argc, char** argv)
{
    int rv = validate_address(argc, argv);
    if (rv < 0) {
        useage();
        return 1;
    }
    rv = set_signals();
    if (rv < 0) {
        fprintf(stderr, "signals failed to be set\n");
        return 1;
    }
    bs = get_udp_socket(NULL, "0");
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

void term_handle(int signum)
{
    if (signum == SIGINT) {
        UFTP_DEBUG_ERR("SIGINT\n");
    } else if (signum == SIGTERM) {
        UFTP_DEBUG_ERR("SIGTERM\n");
    }
    fflush(stdout);
    fflush(stderr);
    close(bs.fd);
    exit(0);
}
int set_signals()
{
    if (signal(SIGINT, &term_handle) == SIG_ERR) {
        fprintf(stderr, "signal for termination failed to be set\n");
        return -1;
    }
    if (signal(SIGTERM, &term_handle) == SIG_ERR) {
        fprintf(stderr, "signal for termination failed to be set\n");
        return -1;
    }
    return 0;
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
