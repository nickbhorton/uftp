#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "String.h"
#include "StringVector.h"
#include "uftp.h"

void useage();

int validate_address(int argc, char** argv);

int get_address(const char* address, const char* port, Address* server_address)
{
    struct addrinfo serv_hints, *serv_res;
    memset(&serv_hints, 0, sizeof(serv_hints));
    serv_hints.ai_family = AF_INET;
    serv_hints.ai_socktype = SOCK_DGRAM;

    int ret;
    ret = getaddrinfo(address, port, &serv_hints, &serv_res);

    if (ret != 0) {
        fprintf(
            stderr,
            "get_address::getaddrinfo() error: %s\n",
            gai_strerror(ret)
        );
        freeaddrinfo(serv_res);
        return -1;
    }
    if (serv_res == NULL) {
        fprintf(
            stderr,
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

int client_exit(int sockfd, Address* server_address)
{
    int rv = 0;
    struct pollfd sock_pfd[1];
    sock_pfd[0].fd = sockfd;
    sock_pfd[0].events = POLLIN | POLLOUT;
    while (1) {
        int num_events = poll(sock_pfd, 1, UFTP_TIMEOUT_MS);
        if (num_events < 0) {
            int en = errno;
            rv = num_events;
            fprintf(stderr, "client_exit::poll() error %s\n", strerror(en));
            break;
        } else if (num_events == 0) {
            fprintf(stderr, "client_exit::poll() timed out\n");
            break;
        }
        if (sock_pfd[0].revents & POLLOUT) {
            // EXT to server
            if ((rv = send_packet(
                     sockfd,
                     server_address,
                     StringView_from_cstr("EXT")
                 )) < 0) {
                break;
            }
            // turn off POLLOUT event
            sock_pfd[0].events = POLLIN;
        }
        if (sock_pfd[0].revents & POLLIN) {
            String packet = String_new();
            if ((rv = recv_packet(sockfd, server_address, &packet, false)) <
                0) {
                String_free(&packet);
                break;
            };
            if (strncmp(packet.data, "GDB", 3) == 0) {
                fprintf(stdout, "server says goodbye!\n");
            } else {
                fprintf(
                    stderr,
                    "client_exit(), strange packet from server found\n"
                );
            }
            String_free(&packet);
            break;
        }
    }
    return rv;
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
        int num_events = poll(server_pollfd, 1, UFTP_TIMEOUT_MS);
        if (num_events != 0) {
            if (server_pollfd[0].revents & POLLIN) {
                String packet;
                int bytes_recv = recv_packet(
                    server_pollfd[0].fd,
                    server_address,
                    &packet,
                    false
                );
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
    uint32_t total_payload_size = 0;
    uint32_t payload_size_hint = 0;

    while (1) {
        if (number_packets_recv == number_packets_to_recv &&
            number_packets_to_recv != 0) {
            break;
        }
        int num_events = poll(sock_poll, 1, UFTP_TIMEOUT_MS);
        if (num_events != 0) {
            if (sock_poll[0].revents & POLLIN) {
                String packet;
                int rv = recv_packet(sock_poll[0].fd, address, &packet, false);
                if (rv < 0) {
                    String_free(&packet);
                    return rv;
                } else {
                    if (strncmp(packet.data, "FNO", UFTP_HEADER_SIZE) == 0) {
                        String_free(&packet);
                        // probably indicates no file on server
                        return -3;
                    }
                    char header[UFTP_HEADER_SIZE];
                    uint32_t seq_number;
                    uint32_t seq_total;
                    String payload;
                    int rv = parse_sequenced_packet(
                        &packet,
                        header,
                        &seq_number,
                        &seq_total,
                        &payload
                    );
                    String_free(&packet);
                    if (rv < 0) {
                        return rv;
                    }

                    number_packets_recv++;
                    // TODO: what if first packet is last file chunk
                    if (payload_size_hint == 0) {
                        payload_size_hint = payload.len;
                        if (UFTP_DEBUG) {
                            printf("payload_size_hint %i\n", payload_size_hint);
                        }
                    }
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
                    total_payload_size += payload.len;
                    String_to_file_chunked(
                        &payload,
                        filename_to_recv,
                        payload.len,
                        payload_size_hint * (seq_number - 1)
                    );
                    if (UFTP_DEBUG) {
                        String_print(filename_to_recv, false);
                        printf(
                            " chunk written %i/%i at %i of size %zu\n",
                            seq_number,
                            seq_total,
                            payload_size_hint * (seq_number - 1),
                            payload.len
                        );
                    }
                    String_free(&payload);
                }
            }
        } else {
            fprintf(stderr, "timed out\n");
            break;
        }
    }
    return 0;
}

int put_file(
    struct pollfd* pfd,
    Address* server_address,
    const StringVector* args,
    const String* cmd
)
{
    int sockfd = pfd[0].fd;
    String filename = String_create(
        StringVector_get(args, 1)->data,
        StringVector_get(args, 1)->len
    );
    struct stat filestats;
    const char* filename_cstr = String_to_cstr(&filename);
    int rv = stat(filename_cstr, &filestats);
    free((void*)filename_cstr);

    if (rv < 0) {
        int stat_err = errno;
        if (stat_err == EBADF || stat_err == EACCES) {
            printf("-uftp_client: ");
            printf(": No such file'\n");
            String_free(&filename);
            return 0;
        } else {
            fprintf(
                stderr,
                "send_file::stat() error: %s\n",
                strerror(stat_err)
            );
            String_free(&filename);
            return -1;
        }
    }
    size_t file_length = filestats.st_size;
    size_t chunk_length = UFTP_BUFFER_SIZE - UFTP_SEQ_PROTOCOL_SIZE;
    size_t chunk_count = file_length / chunk_length + 1;

    // send server the filename and chunk length hint
    rv = send_sequenced_packet(
        sockfd,
        server_address,
        "PFL",
        1,
        chunk_length,
        StringView_create(&filename, 0, filename.len)
    );
    if (rv < 0) {
        String_free(&filename);
        return rv;
    }

    // get ERR or SUC from server before sending file
    while (1) {
        int events = poll(pfd, 1, UFTP_TIMEOUT_MS);
        if (events == 0) {
            printf("timed out\n");
            String_free(&filename);
            return 0;
        } else if (pfd[0].revents & POLLIN) {
            String packet_in = String_new();
            rv = recv_packet(pfd[0].fd, server_address, &packet_in, false);
            if (rv < 0) {
                String_free(&packet_in);
                String_free(&filename);
                return rv;
            } else if (strncmp(packet_in.data, "SUC", 3) == 0) {
                String_free(&packet_in);
                break;
            } else if (strncmp(packet_in.data, "ERR", 3) == 0) {
                printf("server error\n");
                String_free(&packet_in);
                String_free(&filename);
                return 0;
            }
            String_free(&packet_in);
        }
        printf("poll did something unexpected\n");
        return -1;
    }

    // send and recv packets from server
    size_t chunk_index = 0;
    size_t success_count = 0;

    // turn on out and in for socket
    pfd[0].events = POLLIN | POLLOUT;
    while (1) {
        if (success_count == chunk_count) {
            if (UFTP_DEBUG) {
                printf("file sent successfully\n");
            }
            break;
        }
        int events = poll(pfd, 1, UFTP_TIMEOUT_MS);
        if (events == 0) {
            printf(
                "timed out %zu/%zu success progress\n",
                success_count,
                chunk_count
            );
            break;
        } else if (events > 0) {
            // send chunks
            if (pfd->revents & POLLOUT) {
                if (chunk_index < chunk_count - 1) {
                    // send non final chunk
                    String chunk = String_from_file_chunked(
                        &filename,
                        chunk_length,
                        chunk_index * chunk_length
                    );
                    rv = send_sequenced_packet(
                        sockfd,
                        server_address,
                        "FLQ",
                        chunk_index + 1,
                        chunk_count,
                        StringView_create(&chunk, 0, chunk.len)
                    );
                    String_free(&chunk);
                    if (rv < 0) {
                        String_free(&filename);
                        return rv;
                    }
                } else if (chunk_index == chunk_count - 1) {
                    // send final chunk
                    String chunk = String_from_file_chunked(
                        &filename,
                        file_length % chunk_length,
                        chunk_index * chunk_length
                    );
                    rv = send_sequenced_packet(
                        sockfd,
                        server_address,
                        "FLQ",
                        chunk_index + 1,
                        chunk_count, // + 1 for the final chunk
                        StringView_create(&chunk, 0, chunk.len)
                    );
                    String_free(&chunk);
                    if (rv < 0) {
                        String_free(&filename);
                        return rv;
                    }
                } else if (chunk_index == chunk_count) {
                    // turn off sending capabilities
                    pfd->events = POLLIN;
                }
                chunk_index++;
            }
            // recv response from server
            if (pfd->revents & POLLIN) {
                String packet_in = String_new();
                rv = recv_packet(pfd[0].fd, server_address, &packet_in, false);
                if (rv < 0) {
                    String_free(&packet_in);
                    return rv;
                } else if (strncmp(packet_in.data, "SUC", 3) == 0) {
                    success_count++;
                    char header[UFTP_HEADER_SIZE];
                    uint32_t seq_number;
                    uint32_t seq_total;
                    rv = parse_sequenced_packet(
                        &packet_in,
                        header,
                        &seq_number,
                        &seq_total,
                        NULL
                    );
                    if (rv < 0) {
                        String_free(&packet_in);
                        return rv;
                    }
                } else if (strncmp(packet_in.data, "ERR", 3) == 0) {
                    char header[UFTP_HEADER_SIZE];
                    uint32_t seq_number;
                    uint32_t seq_total;
                    rv = parse_sequenced_packet(
                        &packet_in,
                        header,
                        &seq_number,
                        &seq_total,
                        NULL
                    );
                    if (rv < 0) {
                        String_free(&packet_in);
                        return rv;
                    }
                    if (UFTP_DEBUG) {
                        printf("ERR %i/%i\n", seq_number, seq_total);
                    }
                } else {
                    printf("strange packet from server\n");
                }
                String_free(&packet_in);
            }
        } else {
            printf("poll did something unexpected\n");
            return -1;
        }
    }
    // turn of in for socket
    pfd->events = POLLIN;
    return 0;
}

/*
 * returns:
 *  <0 if error
 *   0 if command was not executed
 *   1 indicates program should exit
 */
int execute_command(int sockfd, Address* server_address, StringView cmd)
{
    int rv = 0;
    if (strncmp(cmd.data, "exit", 4) == 0) {
        rv = client_exit(sockfd, server_address);
        if (rv < 0) {
            return rv;
        } else {
            // program should finish
            return 1;
        }
    }

    return rv;
}

int terminal_loop(int sockfd, Address* server_address)
{
    String terminal_input = String_create_of_size('0', UFTP_BUFFER_SIZE);

    struct pollfd cin_pfd[1];
    cin_pfd[0].fd = 0;
    cin_pfd[0].events = POLLIN;

    int rv;

    bool timed_out = false;
    while (true) {
        if (!timed_out) {
            printf("> ");
            fflush(stdout);
        }

        int event_count = poll(cin_pfd, 1, UFTP_TIMEOUT_MS);
        if (event_count == 0) {
            timed_out = true;
            continue;
        } else if (event_count < 0) {
            fprintf(stderr, "server_loop::poll() returned %i\n", event_count);
            break;
        }

        timed_out = false;

        if (cin_pfd[0].revents & POLLIN) {
            // if read fails, exit loop
            if ((rv = read(0, terminal_input.data, UFTP_BUFFER_SIZE)) < 0) {
                int en = errno;
                fprintf(
                    stderr,
                    "terminal_loop::read() return %i, %s\n",
                    rv,
                    strerror(en)
                );
                break;
            }
            // exit command given
            if (strncmp(terminal_input.data, "exit\n", 5) == 0) {
                rv = execute_command(
                    sockfd,
                    server_address,
                    StringView_from_cstr("exit")
                );
                // indicates cleanly exit program
                if (rv == 1) {
                    rv = 0;
                    break;
                }
                // indicates error
                else if (rv < 0) {
                    break;

                }
                // indicates not command was executed
                else if (rv == 0) {
                    fprintf(stderr, "server_loop() exit command did not run\n");
                    rv = -1;
                    break;
                }
            }

            /*
            else if (String_cmp_cstr(&terminal_input, "ls") == 0) {
                rv = send_packet(
                    bs.fd,
                    &server_address,
                    StringView_from_cstr("LST")
                );
                if (rv < 0) {
                    String_free(&cmd);
                    return -rv;
                }
                rv = handle_ls_response(&pfds[1], &server_address);
                if (rv < 0) {
                    String_free(&cmd);
                    return rv;
                }
            } else if (String_cmpn_cstr(&cmd, "get", 3) == 0) {
                StringVector args = StringVector_from_split(&cmd, ' ');
                if (args.len >= 2) {
                    String filename = String_create(
                        StringVector_get(&args, 1)->data,
                        StringVector_get(&args, 1)->len
                    );
                    rv = send_sequenced_packet(
                        bs.fd,
                        &server_address,
                        "GFL",
                        1,
                        1,
                        StringView_create(&filename, 0, filename.len)
                    );
                    if (rv < 0) {
                        String_free(&filename);
                        String_free(&cmd);
                        return -rv;
                    }
                    rv = recieve_file(&pfds[1], &server_address, &filename);
                    if (rv < 0) {
                        String_free(&filename);
                        String_free(&cmd);
                        return rv;
                    } else if (rv == 1) {
                        printf("uftp_client: ");
                        String_print(&filename, false);
                        printf(": No such file\n");
                    }
                    String_free(&filename);
                } else {
                    printf("-uftp_client: ");
                    printf(": usage is 'get <filename>'\n");
                }
                StringVector_free(&args);
            } else if (String_cmpn_cstr(&cmd, "put", 3) == 0) {
                StringVector args = StringVector_from_split(&cmd, ' ');
                if (args.len >= 2) {
                    rv = put_file(&pfds[1], &server_address, &args, &cmd);
                    if (rv < 0) {
                        StringVector_free(&args);
                        String_free(&cmd);
                        return rv;
                    }
                } else {
                    printf("-uftp_client: ");
                    printf(": usage is 'put <filename>'\n");
                }
                StringVector_free(&args);
            }
        */
            else {
                printf("-uftp_client: ");
                String_print_like_cstr(&terminal_input, false);
                printf(": command not found\n");
            }
        }
    }

    // clean up and check rv
    String_free(&terminal_input);
    if (rv < 0) {
        return rv;
    }
    return 0;
}

int execute_commands(
    int sockfd,
    Address* server_address,
    StringVector* commands
)
{
    for (size_t i = 0; i < commands->len; i++) {
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

    int rv;
    Address server_address;
    if ((rv = get_address(argv[1], argv[2], &server_address) < 0)) {
        return -rv;
    }

    if (isatty(STDIN_FILENO)) {
        if ((rv = terminal_loop(bs.fd, &server_address)) < 0) {
            return -rv;
        };
    } else {
        String cin_input = String_create_of_size('\0', UFTP_BUFFER_SIZE);
        if ((rv = read(0, cin_input.data, UFTP_BUFFER_SIZE)) < 0) {
            int en = errno;
            fprintf(stderr, "read() error %s\n", strerror(en));
            String_free(&cin_input);
            return -rv;
        }
        StringVector commands = StringVector_from_split(&cin_input, ';');
        for (size_t i = 0; i < commands.len; i++) {
            String* command = &commands.data[i];
            rv = execute_command(
                bs.fd,
                &server_address,
                StringView_create(command, 0, command->len)
            );
            // indicates error
            if (rv < 0) {
                fprintf(stderr, "execute_command error\n");
                break;
            } else if (rv == 1) {
                rv = 0;
                break;
            }
        }
        if (rv < 0) {
            StringVector_free(&commands);
            String_free(&cin_input);
            return -rv;
        }
        StringVector_free(&commands);
        String_free(&cin_input);
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
