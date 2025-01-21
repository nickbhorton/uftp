#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "String.h"
#include "StringVector.h"
#include "uftp.h"

#define BUFFER_LENGTH 256

#define DEBUG 0

void useage();

int validate_address(int argc, char** argv);

int send_cmd(int sockfd, Address* server_address, String* cmd)
{
    int bytes_sent_or_error;
    if ((bytes_sent_or_error = sendto(
             sockfd,
             cmd->data,
             cmd->len,
             0,
             Address_sockaddr(server_address),
             server_address->addrlen
         )) < 0) {
        int sendto_err = errno;
        fprintf(stderr, "sendto() error: %s\n", strerror(sendto_err));
        return -1;
    } else if (bytes_sent_or_error != cmd->len) {
        fprintf(
            stderr,
            "send_packet() error: %s\n",
            "bytes sent were less than packet length"
        );
        return -2;
    }
    return bytes_sent_or_error;
}
int handle_exit_response(struct pollfd* server_pollfd, Address* server_address)
{
    char msg_buffer[BUFFER_LENGTH];
    memset(msg_buffer, 0, sizeof(msg_buffer));

    while (1) {
        int num_events = poll(server_pollfd, 1, 1000);
        if (num_events != 0 && server_pollfd[0].revents & POLLIN) {
            int bytes_recv = recvfrom(
                server_pollfd[0].fd,
                msg_buffer,
                sizeof(msg_buffer),
                0,
                Address_sockaddr(server_address),
                &server_address->addrlen
            );
            if (bytes_recv > 0) {
                String packet = String_create(msg_buffer, bytes_recv);
                String head = String_create(msg_buffer, 3);
                if (String_cmp_cstr(&head, "GDB") == 0) {
                    printf("server says goodbye!\n");
                    String_free(&packet);
                    String_free(&head);
                    return 0;
                } else {
                    fprintf(
                        stderr,
                        "handle_exit_response, strange packet found\n"
                    );
                    String_print(&head, true);
                    String_dbprint_hex(&packet);
                }
                String_free(&packet);
                String_free(&head);
            } else {
                fprintf(stderr, "handle_exit_response recvfrom error\n");
                return -1;
            }
        } else {
            fprintf(stderr, "timed out\n");
            return 0;
        }
    }
    return 0;
}

int handle_ls_response(struct pollfd* server_pollfd, Address* server_address)
{
    char msg_buffer[BUFFER_LENGTH];
    memset(msg_buffer, 0, sizeof(msg_buffer));

    uint32_t number_filenames_to_recv = 0;
    uint32_t number_filenames_recv = 0;

    while (1) {
        if (number_filenames_recv == number_filenames_to_recv &&
            number_filenames_to_recv != 0) {
            return 0;
        }
        int num_events = poll(server_pollfd, 1, 2500);
        if (num_events != 0) {
            if (server_pollfd[0].revents & POLLIN) {
                int bytes_recv = recvfrom(
                    server_pollfd[0].fd,
                    msg_buffer,
                    sizeof(msg_buffer),
                    0,
                    Address_sockaddr(server_address),
                    &server_address->addrlen
                );
                if (bytes_recv > 0) {
                    String packet = String_create(msg_buffer, bytes_recv);
                    String head = String_create(msg_buffer, 3);
                    if (String_cmp_cstr(&head, "FLS") == 0 && packet.len == 7) {
                        number_filenames_to_recv =
                            ntohl(String_parse_u32(&packet, 3));
                        if (DEBUG) {
                            printf(
                                "to recv %u filenames\n",
                                number_filenames_to_recv
                            );
                        }
                    } else if (String_cmp_cstr(&head, "FLN") == 0 &&
                               packet.len >= 11) {
                        uint32_t packet_length =
                            ntohl(String_parse_u32(&packet, 3));
                        if (packet.len != packet_length) {
                            fprintf(
                                stderr,
                                "FLN packet was the wrong size, %u, %zu\n",
                                packet_length,
                                packet.len
                            );
                        }
                        uint32_t packet_seq =
                            ntohl(String_parse_u32(&packet, 7));
                        String filename =
                            String_create(packet.data + 11, packet.len - 11);
                        String_print(&filename, false);
                        if (DEBUG) {
                            printf(" %u\n", packet_seq);
                        } else {
                            printf("\n");
                        }
                        String_free(&filename);
                        number_filenames_recv++;
                    } else {
                        fprintf(
                            stderr,
                            "handle_ls_response, strange packet found\n"
                        );
                        String_print(&head, true);
                        String_dbprint_hex(&packet);
                    }
                    String_free(&packet);
                    String_free(&head);
                }
            }
        } else {
            fprintf(stderr, "timed out\n");
            return 0;
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    if (validate_address(argc, argv) < 0) {
        return 1;
    }
    UdpBoundSocket bs = get_udp_socket(NULL, "0");
    if (bs.fd < 0) {
        return -bs.fd;
    }

    struct addrinfo serv_hints, *serv_res;
    memset(&serv_hints, 0, sizeof(serv_hints));
    serv_hints.ai_family = AF_INET;
    serv_hints.ai_socktype = SOCK_DGRAM;

    int ret;
    ret = getaddrinfo(argv[1], argv[2], &serv_hints, &serv_res);

    if (ret != 0) {
        fprintf(stderr, "server getaddrinfo() error: %s\n", gai_strerror(ret));
        freeaddrinfo(serv_res);
        return 1;
    }
    if (serv_res == NULL) {
        fprintf(stderr, "faild to find server address info\n");
        freeaddrinfo(serv_res);
        return 1;
    }

    Address server_address;
    server_address.addrlen = serv_res->ai_addrlen;
    memcpy(&server_address.addr, serv_res->ai_addr, server_address.addrlen);

    freeaddrinfo(serv_res);

    struct pollfd pfds[2];
    pfds[0].fd = 0;
    pfds[0].events = POLLIN;
    pfds[1].fd = bs.fd;
    pfds[1].events = POLLIN;

    char msg_buffer[BUFFER_LENGTH];
    memset(msg_buffer, 0, sizeof(msg_buffer));

    struct sockaddr_storage server_addr;
    socklen_t server_addr_len;
    int bytes_recv;
    bool timed_out = false;
    while (1) {
        if (!timed_out) {
            printf("> ");
            fflush(stdout);
        }
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr_len = sizeof(server_addr);

        int num_events = poll(pfds, 2, 2500);
        if (num_events != 0) {
            timed_out = false;
            if (pfds[0].revents & POLLIN) {
                bytes_recv = read(0, msg_buffer, sizeof(msg_buffer));
                if (bytes_recv > 1) {
                    // -1 to remove the \n for line buffered terminal?
                    String cmd = String_create(msg_buffer, bytes_recv - 1);
                    if (String_cmp_cstr(&cmd, "ls") == 0) {
                        String ls_command = String_from_cstr("LST");
                        int bs_or_err =
                            send_cmd(bs.fd, &server_address, &ls_command);
                        if (bs_or_err < 0) {
                            String_free(&ls_command);
                            return bs_or_err;
                        }
                        String_free(&ls_command);
                        bs_or_err =
                            handle_ls_response(&pfds[1], &server_address);
                        if (bs_or_err < 0) {
                            return bs_or_err;
                        }
                    } else if (String_cmp_cstr(&cmd, "exit") == 0) {
                        String ext_command = String_from_cstr("EXT");
                        int bs_or_err =
                            send_cmd(bs.fd, &server_address, &ext_command);
                        if (bs_or_err < 0) {
                            String_free(&ext_command);
                            return -bs_or_err;
                        }
                        bs_or_err =
                            handle_exit_response(&pfds[1], &server_address);
                        if (bs_or_err < 0) {
                            return -bs_or_err;
                        }
                        String_free(&ext_command);
                        String_free(&cmd);
                        // exit from while loop
                        break;
                    } else if (String_cmpn_cstr(&cmd, "get", 3) == 0) {
                        StringVector sv = StringVector_from_split(&cmd, ' ');
                        if (sv.len >= 2) {
                            String filename = String_create(
                                StringVector_get(&sv, 1)->data,
                                StringVector_get(&sv, 1)->len
                            );
                            String_print(&filename, true);
                            String_free(&filename);
                        } else {
                            printf("-uftp_client: ");
                            printf(": usage is 'get <filename>'\n");
                        }
                        StringVector_free(&sv);
                    } else {
                        printf("-uftp_client: ");
                        String_print(&cmd, false);
                        printf(": command not found\n");
                    }
                    String_free(&cmd);
                } else {
                    int read_err = errno;
                    fprintf(stderr, "read() error: %s\n", strerror(read_err));
                    close(bs.fd);
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
                    String packet = String_create(msg_buffer, bytes_recv);
                    String head = String_create(packet.data, 3);
                    String_print(&head, true);
                    String_dbprint_hex(&packet);
                    String_free(&packet);
                    String_free(&head);
                }
            }
        } else {
            timed_out = true;
        }
        memset(msg_buffer, 0, sizeof(msg_buffer));
    }
    close(bs.fd);
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
