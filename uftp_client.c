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

#define DEBUG 0

static int timeout_ms = 1000;

void useage();

int validate_address(int argc, char** argv);

int handle_exit_response(struct pollfd* server_pollfd, Address* server_address)
{
    while (1) {
        int num_events = poll(server_pollfd, 1, timeout_ms);
        if (num_events != 0 && server_pollfd[0].revents & POLLIN) {
            String packet;
            int bytes_recv =
                recv_packet(server_pollfd[0].fd, server_address, &packet);
            if (bytes_recv < 0) {
                String_free(&packet);
                return bytes_recv;
            } else {
                if (String_cmp_cstr(&packet, "GDB") == 0) {
                    printf("server says goodbye!\n");
                } else {
                    fprintf(
                        stderr,
                        "handle_exit_response, strange packet found\n"
                    );
                }
                String_free(&packet);
                return 0;
            }
        } else {
            fprintf(stderr, "timed out\n");
            return 0;
        }
    }
    return 0;
}

// TODO: request packets again if they are dropped
int handle_ls_response(struct pollfd* server_pollfd, Address* server_address)
{
    uint32_t number_filenames_to_recv = 0;
    uint32_t number_filenames_recv = 0;

    while (1) {
        if (number_filenames_recv == number_filenames_to_recv &&
            number_filenames_to_recv != 0) {
            return 0;
        }
        int num_events = poll(server_pollfd, 1, timeout_ms);
        if (num_events != 0) {
            if (server_pollfd[0].revents & POLLIN) {
                String packet;
                int bytes_recv =
                    recv_packet(server_pollfd[0].fd, server_address, &packet);
                if (bytes_recv < 0) {
                    String_free(&packet);
                    return bytes_recv;
                } else {
                    char header[UFTP_HEADER_SIZE];
                    uint32_t seq_number;
                    uint32_t seq_total;
                    String payload;
                    int ret = parse_sequenced_packet(
                        &packet,
                        header,
                        &seq_number,
                        &seq_total,
                        &payload
                    );
                    String_free(&packet);
                    if (ret < 0) {
                        return ret;
                    }
                    number_filenames_recv++;
                    if (strncmp(header, "LSQ", UFTP_HEADER_SIZE) != 0) {
                        // not a critical error
                        fprintf(
                            stderr,
                            "sequenced packet header for ls was not correct\n"
                        );
                    }
                    if (number_filenames_to_recv == 0) {
                        number_filenames_to_recv = seq_total;
                    } else if (seq_total != number_filenames_to_recv) {
                        // not a critical error
                        fprintf(
                            stderr,
                            "sequenced packet seq_total did not match\n"
                        );
                    }
                    String_print(&payload, true);
                    String_free(&payload);
                }
            }
        } else {
            fprintf(stderr, "timed out\n");
            return 0;
        }
    }
    return 0;
}

// TODO: request dropped packets
int recieve_file(
    struct pollfd* sock_poll,
    Address* address,
    String* filename_to_recv
)
{
    uint32_t number_packets_to_recv = 0;
    uint32_t number_packets_recv = 0;
    StringVector file = StringVector_new();

    while (1) {
        if (number_packets_recv == number_packets_to_recv &&
            number_packets_to_recv != 0) {
            break;
        }
        int num_events = poll(sock_poll, 1, timeout_ms);
        if (num_events != 0) {
            if (sock_poll[0].revents & POLLIN) {
                String packet;
                int bytes_recv = recv_packet(sock_poll[0].fd, address, &packet);
                if (bytes_recv < 0) {
                    String_free(&packet);
                    StringVector_free(&file);
                    return bytes_recv;
                } else {
                    char header[UFTP_HEADER_SIZE];
                    uint32_t seq_number;
                    uint32_t seq_total;
                    String payload;
                    int ret = parse_sequenced_packet(
                        &packet,
                        header,
                        &seq_number,
                        &seq_total,
                        &payload
                    );
                    String_free(&packet);
                    if (ret < 0) {
                        StringVector_free(&file);
                        return ret;
                    }
                    number_packets_recv++;
                    if (strncmp(header, "FLQ", UFTP_HEADER_SIZE) != 0) {
                        // not a critical error
                        fprintf(
                            stderr,
                            "sequenced packet header for get was not correct\n"
                        );
                    }
                    if (number_packets_to_recv == 0) {
                        number_packets_to_recv = seq_total;
                    } else if (seq_total != number_packets_to_recv) {
                        // not a critical error
                        fprintf(
                            stderr,
                            "sequenced packet seq_total did not match\n"
                        );
                    }
                    if (DEBUG) {
                        String_print(&payload, true);
                    }
                    StringVector_push_back_move(&file, payload);
                }
            }
        } else {
            fprintf(stderr, "timed out\n");
            break;
        }
    }
    String file_content = String_new();
    for (size_t i = 0; i < file.len; i++) {
        String content = String_create(
            StringVector_get(&file, i)->data,
            StringVector_get(&file, i)->len
        );
        String_push_move(&file_content, content);
    }
    StringVector_free(&file);

    // to file
    String debug_filename = String_from_cstr("out.client");
    String_to_file(&file_content, &debug_filename);
    String_free(&debug_filename);
    String_free(&file_content);
    return 0;
}

// TODO: handle dealocations on error
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

    char msg_buffer[UFTP_BUFFER_SIZE];
    memset(msg_buffer, 0, sizeof(msg_buffer));

    Address server;
    int bytes_recv;
    bool timed_out = false;
    while (1) {
        if (!timed_out) {
            printf("> ");
            fflush(stdout);
        }
        int num_events = poll(pfds, 2, timeout_ms);
        if (num_events != 0) {
            timed_out = false;
            if (pfds[0].revents & POLLIN) {
                bytes_recv = read(0, msg_buffer, sizeof(msg_buffer));
                if (bytes_recv > 1) {
                    // -1 to remove the \n for line buffered terminal?
                    String cmd = String_create(msg_buffer, bytes_recv - 1);
                    if (String_cmp_cstr(&cmd, "ls") == 0) {
                        int bs_or_err = send_packet(
                            bs.fd,
                            &server_address,
                            StringView_from_cstr("LST")
                        );
                        if (bs_or_err < 0) {
                            String_free(&cmd);
                            return -bs_or_err;
                        }
                        bs_or_err =
                            handle_ls_response(&pfds[1], &server_address);
                        if (bs_or_err < 0) {
                            String_free(&cmd);
                            return bs_or_err;
                        }
                    } else if (String_cmp_cstr(&cmd, "exit") == 0) {
                        int bs_or_err = send_packet(
                            bs.fd,
                            &server_address,
                            StringView_from_cstr("EXT")
                        );
                        if (bs_or_err < 0) {
                            return -bs_or_err;
                        }
                        bs_or_err =
                            handle_exit_response(&pfds[1], &server_address);
                        if (bs_or_err < 0) {
                            return -bs_or_err;
                        }
                        String_free(&cmd);
                        // exit from while loop
                        break;
                    } else if (String_cmpn_cstr(&cmd, "get", 3) == 0) {
                        StringVector get_args =
                            StringVector_from_split(&cmd, ' ');
                        if (get_args.len >= 2) {
                            String filename = String_create(
                                StringVector_get(&get_args, 1)->data,
                                StringVector_get(&get_args, 1)->len
                            );
                            int bs_or_err = send_sequenced_packet(
                                bs.fd,
                                &server_address,
                                "GFL",
                                1,
                                1,
                                StringView_create(&filename, 0, filename.len)
                            );
                            if (bs_or_err < 0) {
                                String_free(&filename);
                                String_free(&cmd);
                                return -bs_or_err;
                            }
                            bs_or_err = recieve_file(
                                &pfds[1],
                                &server_address,
                                &filename
                            );
                            String_free(&filename);
                            if (bs_or_err < 0) {
                                String_free(&cmd);
                                return bs_or_err;
                            }
                        } else {
                            printf("-uftp_client: ");
                            printf(": usage is 'get <filename>'\n");
                        }
                        StringVector_free(&get_args);
                    } else if (String_cmpn_cstr(&cmd, "put", 3) == 0) {
                        StringVector get_args =
                            StringVector_from_split(&cmd, ' ');
                        if (get_args.len >= 2) {
                            /*
                            String filename = String_create(
                                StringVector_get(&get_args, 1)->data,
                                StringVector_get(&get_args, 1)->len
                            );
                            */
                            String filename =
                                String_from_cstr("smnt/files/basic.txt");
                            int bs_or_err = send_sequenced_packet(
                                bs.fd,
                                &server_address,
                                "PFL",
                                1,
                                1,
                                StringView_create(&filename, 0, filename.len)
                            );
                            if (bs_or_err < 0) {
                                String_free(&filename);
                                String_free(&cmd);
                                return -bs_or_err;
                            }
                            String contents = String_from_file(&filename);
                            bs_or_err = send_sequenced_packet(
                                bs.fd,
                                &server_address,
                                "FLQ",
                                1,
                                1,
                                StringView_create(&contents, 0, contents.len)
                            );
                            if (bs_or_err < 0) {
                                String_free(&contents);
                                String_free(&filename);
                                String_free(&cmd);
                                return bs_or_err;
                            }
                            String_free(&contents);
                            String_free(&filename);
                        } else {
                            printf("-uftp_client: ");
                            printf(": usage is 'get <filename>'\n");
                        }
                        StringVector_free(&get_args);
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
                String packet;
                bytes_recv = recv_packet(pfds[1].fd, &server, &packet);
                if (bytes_recv > 0) {
                    String_dbprint_hex(&packet);
                    String_free(&packet);
                } else {
                    String_free(&packet);
                    return 1;
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
