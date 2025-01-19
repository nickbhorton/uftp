#include "mstring.h"
#include "uftp.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

void useage();

int validate_address(int argc, char** argv);

int bind_client_socket(int* sockfd_o);

int main(int argc, char** argv)
{
    if (validate_address(argc, argv) < 0) {
        return 1;
    }
    int sockfd = get_udp_socket(NULL, "0");

    struct addrinfo serv_hints, *serv_res;
    memset(&serv_hints, 0, sizeof(serv_hints));
    serv_hints.ai_family = AF_INET;
    serv_hints.ai_socktype = SOCK_DGRAM;

    int ret;
    ret = getaddrinfo(argv[1], argv[2], &serv_hints, &serv_res);
    if (ret != 0) {
        fprintf(stderr, "server getaddrinfo() error: %s\n", gai_strerror(ret));
        return 1;
    }

    struct pollfd pfds[2];
    pfds[0].fd = 0;
    pfds[0].events = POLLIN;
    pfds[1].fd = sockfd;
    pfds[1].events = POLLIN;

    char msg_buffer[128];
    memset(msg_buffer, 0, sizeof(msg_buffer));

    struct sockaddr_storage server_addr;
    socklen_t server_addr_len;
    int bytes_recv;
    while (1) {
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr_len = sizeof(server_addr);

        int num_events = poll(pfds, 2, 2500);
        if (num_events != 0) {
            if (pfds[0].revents & POLLIN) {
                bytes_recv = read(0, msg_buffer, sizeof(msg_buffer));
                if (bytes_recv > 0) {
                    mstring_t cmd;
                    mstring_init(&cmd);
                    // -1 to remove the \n
                    mstring_appendn(&cmd, msg_buffer, bytes_recv - 1);
                    if (mstring_cmp_cstr(&cmd, "ls") == 0) {
                        printf("requesting list from server\n");
                        int bytes_sent;
                        if ((bytes_sent = sendto(
                                 sockfd,
                                 "LST",
                                 3,
                                 0,
                                 serv_res->ai_addr,
                                 serv_res->ai_addrlen
                             )) < 0) {
                            int sendto_err = errno;
                            fprintf(
                                stderr,
                                "sendto() error: %s\n",
                                strerror(sendto_err)
                            );
                            return 1;
                        }
                    } else {
                        printf("-uftp_client: ");
                        mstring_print(&cmd);
                        printf(": command not found\n");
                    }
                    mstring_free(&cmd);
                } else {
                    int read_err = errno;
                    fprintf(stderr, "read() error: %s\n", strerror(read_err));
                    close(sockfd);
                    return 1;
                }
            } else if (pfds[1].revents & POLLIN) {
                bytes_recv = recvfrom(
                    pfds[1].fd,
                    msg_buffer,
                    sizeof(msg_buffer),
                    0,
                    (struct sockaddr*)&server_addr,
                    &server_addr_len
                );
                if (bytes_recv > 0) {
                    mstring_t packet;
                    mstring_init(&packet);
                    mstring_appendn(&packet, msg_buffer, bytes_recv);
                    mstring_print(&packet);
                    printf("\n");
                    mstring_free(&packet);
                }
            }
        }
        memset(msg_buffer, 0, sizeof(msg_buffer));
    }
    close(sockfd);
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
