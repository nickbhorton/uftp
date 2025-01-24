#include "uftp_server.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "String.h"
#include "StringVector.h"
#include "uftp.h"

void useage();
int validate_port(int argc, char** argv);

int server_loop(UdpBoundSocket* bs, StringVector* filenames);

void print_input(Address* client, String* packet);

int handle_input(
    struct pollfd* sock_poll,
    Address* client,
    String* packet_recv,
    StringVector* filenames
);

int main(int argc, char** argv)
{
    if (validate_port(argc, argv) < 0) {
        return 1;
    }

    String ls_output;
    get_shell_cmd_cout("ls", &ls_output);
    StringVector filenames = StringVector_from_split(&ls_output, '\n');
    String_free(&ls_output);

    UdpBoundSocket bs;
    if ((bs = get_udp_socket(NULL, argv[1])).fd < 0) {
        StringVector_free(&filenames);
        return 1;
    }

    if (server_loop(&bs, &filenames) < 0) {
        close(bs.fd);
    }

    StringVector_free(&filenames);
    return 0;
}

int send_s_packet(int sockfd, Address* to, uint32_t count, const char* outcmd)
{
    String packet = String_create((char*)outcmd, 3);
    String_push_u32(&packet, htonl(count));
    int bytes_sent_or_error =
        send_packet(sockfd, to, StringView_create(&packet, 0, packet.len));
    String_free(&packet);
    return bytes_sent_or_error;
}

int send_n_packet(
    int sockfd,
    Address* to,
    String* payload,
    uint32_t seq,
    const char* outcmd
)
{
    String packet = String_create((char*)outcmd, 3);

    // 2 uint32_t for length and seq
    String_push_u32(
        &packet,
        htonl(payload->len + strlen(outcmd) + sizeof(uint32_t) * 2)
    );
    String_push_u32(&packet, htonl(seq));
    String_push_copy(&packet, payload);

    int bytes_sent_or_error =
        send_packet(sockfd, to, StringView_create(&packet, 0, packet.len));
    String_free(&packet);
    return bytes_sent_or_error;
    return 0;
}

int send_file(
    int sockfd,
    Address* client,
    String* filename,
    StringVector* valid_filenames
)
{
    int rv;
    // if this file exists but is not in the list of valid filesnames also tell
    // the client there is no file. the valid_filenames comes from running a
    // regular ls in the working directory. This prevents the client from giving
    // relative paths or absolute paths into potenetially sensitive data.
    bool valid = false;
    for (size_t i = 0; i < valid_filenames->len; i++) {
        int cmp_val = String_cmp(&valid_filenames->data[i], filename);
        if (cmp_val == 0) {
            valid = true;
        }
    }
    if (!valid) {
        if (UFTP_DEBUG) {
            String_print(filename, false);
            printf(" is not a valid filename\n");
        }
        rv = send_packet(sockfd, client, StringView_from_cstr("FNO"));
        return rv;
    }

    if (UFTP_DEBUG) {
        String_print(filename, false);
        printf(" is valid filename\n");
    }

    struct stat filestats;
    const char* filename_cstr = String_to_cstr(filename);
    rv = stat(filename_cstr, &filestats);
    free((void*)filename_cstr);

    if (rv < 0) {
        int stat_err = errno;
        if (stat_err == EBADF || stat_err == EACCES) {
            rv = send_packet(sockfd, client, StringView_from_cstr("FNO"));
            return rv;
        } else {
            fprintf(
                stderr,
                "send_file::stat() error: %s\n",
                strerror(stat_err)
            );
            return -stat_err;
        }
    }

    size_t file_length = filestats.st_size;
    if (file_length == 0) {
        fprintf(stderr, "send_file() error: file length was zero\n");
    }
    size_t file_chunk_size = UFTP_BUFFER_SIZE - UFTP_SEQ_PROTOCOL_SIZE;
    size_t total_seq = (file_length / file_chunk_size) + 1;
    int total_sent = 0;

    for (size_t i = 0; i < total_seq; i++) {
        String contents =
            String_from_file_chunked(filename, file_chunk_size, i);
        // its also possible that the file does not exist
        rv = send_sequenced_packet(
            sockfd,
            client,
            "FLQ",
            i + 1,
            total_seq,
            StringView_create(&contents, 0, contents.len)
        );
        if (rv < 0) {
            return rv;
        }
        total_sent += rv;
        String_free(&contents);
    }
    if (UFTP_DEBUG) {
        printf("chunk count: %zu\n", total_seq);
        printf("bytes sent: %i\n", total_sent);
        String_print(filename, false);
        printf(" size %zu\n", file_length);
        printf(
            "file bytes sent: %lu\n",
            total_sent - (UFTP_SEQ_PROTOCOL_SIZE * total_seq)
        );
    }
    return total_sent;
}

int recieve_file(
    struct pollfd* sock_poll,
    Address* address,
    String* filename_to_recv
)
{
    char msg_buffer[UFTP_BUFFER_SIZE];
    memset(msg_buffer, 0, sizeof(msg_buffer));

    bool err = false;

    uint32_t number_packets_to_recv = 0;
    uint32_t number_packets_recv = 0;
    StringVector file = StringVector_new();

    while (1) {
        if (number_packets_recv == number_packets_to_recv &&
            number_packets_to_recv != 0) {
            break;
        }
        int num_events = poll(sock_poll, 1, UFTP_TIMEOUT_MS);
        if (num_events != 0) {
            if (sock_poll[0].revents & POLLIN) {
                String packet;
                int bytes_recv =
                    recv_packet(sock_poll[0].fd, address, &packet, false);
                number_packets_recv++;
                if (bytes_recv < 0) {
                    err = true;
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
                    String_dbprint_hex(&packet);
                    String_free(&packet);
                    if (ret < 0) {
                        err = true;
                    } else {
                        if (strncmp(header, "FLQ", UFTP_HEADER_SIZE) != 0) {
                            // not a critical error
                            fprintf(
                                stderr,
                                "sequenced packet header for get was not "
                                "correct\n"
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
                        StringVector_push_back_move(&file, payload);
                    }
                }
            }
        } else {
            fprintf(stderr, "timed out\n");
            err = true;
            break;
        }
    }
    if (err) {
        StringVector_free(&file);
        int ret =
            send_packet(sock_poll[0].fd, address, StringView_from_cstr("ERR"));
        if (ret < 0) {
            return -1;
        }
        return 0;
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
    String_to_file(&file_content, filename_to_recv);
    String_free(&file_content);
    int ret =
        send_packet(sock_poll[0].fd, address, StringView_from_cstr("SUC"));
    if (ret < 0) {
        return ret;
    }
    return 0;
}

int handle_input(
    struct pollfd* sock_poll,
    Address* client,
    String* packet_recv,
    StringVector* filenames
)
{
    int rv;
    // exit
    if (strncmp("EXT", packet_recv->data, 3) == 0) {
        rv = send_packet(sock_poll->fd, client, StringView_from_cstr("GDB"));
        if (rv < 0) {
            return rv;
        }
    }
    // ls
    else if (strncmp("LST", packet_recv->data, 3) == 0) {
        for (size_t i = 0; i < filenames->len; i++) {
            rv = send_sequenced_packet(
                sock_poll->fd,
                client,
                "LSQ",
                i + 1,
                filenames->len,
                StringView_create(
                    &filenames->data[i],
                    0,
                    filenames->data[i].len
                )
            );
            if (rv < 0) {
                return rv;
            }
        }
    }
    // get 'filename' from server
    else if (strncmp("GFL", packet_recv->data, 3) == 0) {
        char header[UFTP_HEADER_SIZE];
        uint32_t seq_number;
        uint32_t seq_total;
        String filename = String_new();
        rv = parse_sequenced_packet(
            packet_recv,
            header,
            &seq_number,
            &seq_total,
            &filename
        );
        if (rv < 0) {
            String_free(&filename);
            rv = send_packet(
                sock_poll[0].fd,
                client,
                StringView_from_cstr("ERR")
            );
            if (rv < 0) {
                return -1;
            }
            return rv;
        }
        rv = send_file(sock_poll->fd, client, &filename, filenames);
        String_free(&filename);
        return rv;
    }
    // put 'filename' onto server
    else if (strncmp("PFL", packet_recv->data, 3) == 0) {
        char header[UFTP_HEADER_SIZE];
        uint32_t seq_number;
        uint32_t seq_total;
        String filename = String_new();
        rv = parse_sequenced_packet(
            packet_recv,
            header,
            &seq_number,
            &seq_total,
            &filename
        );
        if (rv < 0) {
            String_free(&filename);
            rv = send_packet(
                sock_poll[0].fd,
                client,
                StringView_from_cstr("ERR")
            );
            if (rv < 0) {
                return -1;
            }
            return rv;
        }
        rv = recieve_file(sock_poll, client, &filename);
        if (rv < 0) {
            String_free(&filename);
            return rv;
        }
        StringVector_push_back_move(filenames, filename);
        return rv;
    }
    // unknown command, send error
    else {
        rv = send_packet(sock_poll->fd, client, StringView_from_cstr("ERR"));
        if (rv < 0) {
            return rv;
        }
    }
    return 0;
}

int server_loop(UdpBoundSocket* bs, StringVector* filenames)
{
    struct pollfd pfds[1];
    pfds[0].fd = bs->fd;
    pfds[0].events = POLLIN;

    client_list_t cl;
    client_list_init(&cl);

    Address client;
    int rv;
    while (1) {
        // Every loop client address is reset
        memset(&client, 0, sizeof(client));
        client.addrlen = sizeof(client.addrlen);

        int pevents = poll(pfds, 1, UFTP_TIMEOUT_MS);
        if (pevents != 0) {
            if (pfds[0].revents & POLLIN) {
                String packet_in = String_new();
                rv = recv_packet(pfds[0].fd, &client, &packet_in, false);
                if (rv < 0 && UFTP_DEBUG) {
                    fprintf(stderr, "poll() unecpected event\n");
                } else {
                    Address* current_client =
                        get_client(&cl, &client.addr, client.addrlen);
                    if (UFTP_DEBUG) {
                        print_input(current_client, &packet_in);
                    }
                    rv = handle_input(
                        &pfds[0],
                        current_client,
                        &packet_in,
                        filenames
                    );
                    if (rv < 0) {
                        String_free(&packet_in);
                        client_list_free(&cl);
                        return -1;
                    }
                }
                String_free(&packet_in);
            } else {
                fprintf(stderr, "poll() unecpected event\n");
            }
        }
    }
    client_list_free(&cl);
    return 0;
}

void print_input(Address* client, String* packet)
{
    char client_str_buffer[INET6_ADDRSTRLEN];
    memset(&client_str_buffer, 0, sizeof(client_str_buffer));
    uint16_t portnum = Address_port(client);
    const char* client_address_string = inet_ntop(
        client->addr.ss_family,
        Address_in_addr(client),
        client_str_buffer,
        sizeof(client_str_buffer)
    );
    printf("%s:%u(%zu):\n", client_address_string, portnum, packet->len);
    String_dbprint_hex(packet);
}

int validate_port(int argc, char** argv)
{
    if (argc != 2) {
        useage();
        return -1;
    }
    unsigned short int port;
    if (sscanf(argv[1], "%hu", &port) != 1) {
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
        "usage: uftp_server port\n  port: the port you want the server to "
        "listen on\n"
    );
}
