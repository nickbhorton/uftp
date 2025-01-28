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

int server_loop(UdpBoundSocket* bs, StringVector* filenames);

int handle_input(
    int sockfd,
    Client* client,
    String* packet_recv,
    StringVector* filenames
);

int send_file(
    int sockfd,
    const Address* client,
    const String* filename,
    const StringVector* valid_filenames
);

void useage();
int validate_port(int argc, char** argv);
void print_input(Client* client, String* packet);

int main(int argc, char** argv)
{
    if (validate_port(argc, argv) < 0) {
        return 1;
    }

    String ls_output;
    if (get_shell_cmd_cout("ls", &ls_output) < 0) {
        return 1;
    }
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
                    fprintf(stderr, "packet recv failed\n");
                } else {
                    Client* current_client =
                        get_client(&cl, &client.addr, client.addrlen);
                    if (UFTP_DEBUG) {
                        print_input(current_client, &packet_in);
                    }
                    rv = handle_input(
                        pfds[0].fd,
                        current_client,
                        &packet_in,
                        filenames
                    );
                    if (rv < 0) {
                        String_free(&packet_in);
                        client_list_free(&cl);
                        return rv;
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

int send_ERR(int sockfd, const Address* client)
{
    printf("sent ERR\n");
    return send_packet(sockfd, client, StringView_from_cstr("ERR"));
}

int send_SUC(int sockfd, const Address* client)
{
    return send_packet(sockfd, client, StringView_from_cstr("SUC"));
}

int handle_EXT(int sockfd, const Address* client)
{
    return send_packet(sockfd, client, StringView_from_cstr("GDB"));
}

int handle_LST(int sockfd, const Address* client, const StringVector* filenames)
{
    int rv;
    int total = 0;
    for (size_t i = 0; i < filenames->len; i++) {
        rv = send_sequenced_packet(
            sockfd,
            client,
            "LSQ",
            i + 1,
            filenames->len,
            StringView_create(&filenames->data[i], 0, filenames->data[i].len)
        );
        if (rv < 0) {
            return -2;
        }
        total += rv;
    }

    if (UFTP_DEBUG) {
        printf("handle_LST bytes sent: %i\n", total);
    }
    return total;
}

// returns:
//   -1 if send_file fails
//   -2 if send_ERR fails
//   >=0 if no error and indicates bytes sent to client
int handle_GFL(
    int sockfd,
    const Address* client,
    const String* packet_recv,
    const StringVector* filenames
)
{
    char header[UFTP_HEADER_SIZE];
    uint32_t seq_number;
    uint32_t seq_total;
    String filename = String_new();
    int rv = parse_sequenced_packet(
        packet_recv,
        header,
        &seq_number,
        &seq_total,
        &filename
    );
    if (rv < 0) {
        String_free(&filename);
        rv = send_ERR(sockfd, client);
        if (rv < 0) {
            return -2;
        }
        return rv;
    }
    rv = send_file(sockfd, client, &filename, filenames);
    if (UFTP_DEBUG) {
        printf("handle_GFL bytes send %i\n", rv);
    }
    String_free(&filename);
    return rv;
}

int handle_PFL(
    int sockfd,
    Client* client,
    const String* packet_recv,
    const StringVector* filenames
)
{
    char header[UFTP_HEADER_SIZE];
    uint32_t seq_number;
    uint32_t seq_total;
    String filename = String_new();
    int rv = parse_sequenced_packet(
        packet_recv,
        header,
        &seq_number,
        &seq_total,
        &filename
    );
    // repurpose seq_total for a chunk size hint in case the first FLQ
    // packet to be recv is the last one with an unknow size
    if (seq_total > 1) {
        client->file_chunk_size_hint = seq_total;
    }
    if (rv < 0) {
        String_free(&filename);
        rv = send_ERR(sockfd, &client->address);
        if (rv < 0) {
            return -2;
        }
        return rv;
    }
    // if there is already a file dont overwrite it
    bool valid_filename = true;
    for (size_t i = 0; i < filenames->len; i++) {
        int cmp_val = String_cmp(&filenames->data[i], &filename);
        if (cmp_val == 0) {
            valid_filename = false;
        }
    }
    if (!valid_filename) {
        // file overwrite
        String_free(&filename);
        rv = send_ERR(sockfd, &client->address);
        if (rv < 0) {
            return -2;
        }
        return rv;
    }

    if (client->writing_filename.len > 0) {
        String_free(&client->writing_filename);
        client->writing_filename = String_new();
    }
    if (UFTP_DEBUG) {
        printf(
            "client at port %i writting_file bound ",
            Address_port(&client->address)
        );
        String_print(&filename, true);
    }
    String_push_move(&client->writing_filename, filename);
    rv = send_SUC(sockfd, &client->address);
    if (rv < 0) {
        return -2;
    }
    return rv;
}

int handle_FLQ(
    int sockfd,
    Client* client,
    const String* packet_recv,
    const StringVector* filenames
)
{
    char header[UFTP_HEADER_SIZE];
    uint32_t seq_number = 0;
    uint32_t seq_total = 0;
    String file_content_chunk = String_new();
    int rv = parse_sequenced_packet(
        packet_recv,
        header,
        &seq_number,
        &seq_total,
        &file_content_chunk
    );
    if (rv < 0) {
        String_free(&file_content_chunk);
        rv = send_sequenced_packet(
            sockfd,
            &client->address,
            "ERR",
            seq_number,
            seq_total,
            StringView_from_cstr("")
        );
        if (rv < 0) {
            return -2;
        }
        return rv;
    }
    // if there is no filename bound to client send error
    if (client->writing_filename.len < 0) {
        String_free(&file_content_chunk);
        rv = send_sequenced_packet(
            sockfd,
            &client->address,
            "ERR",
            seq_number,
            seq_total,
            StringView_from_cstr("")
        );
        if (rv < 0) {
            return -2;
        }
        return rv;
    }

    // if last chunk do special calculation for position
    if (seq_number == seq_total) {
        rv = String_to_file_chunked(
            &file_content_chunk,
            &client->writing_filename,
            file_content_chunk.len,
            (seq_number - 1) * client->file_chunk_size_hint
        );
        if (rv < 0) {
            String_free(&file_content_chunk);
            rv = send_sequenced_packet(
                sockfd,
                &client->address,
                "ERR",
                seq_number,
                seq_total,
                StringView_from_cstr("")
            );
            if (rv < 0) {
                return -2;
            }
            return rv;
        }
    } else {
        rv = String_to_file_chunked(
            &file_content_chunk,
            &client->writing_filename,
            file_content_chunk.len,
            (seq_number - 1) * file_content_chunk.len
        );
        if (rv < 0) {
            String_free(&file_content_chunk);
            rv = send_sequenced_packet(
                sockfd,
                &client->address,
                "ERR",
                seq_number,
                seq_total,
                StringView_from_cstr("")
            );
            if (rv < 0) {
                return -2;
            }
            return rv;
        }
    }
    if (seq_total < 8 || seq_number % 1000 == 0 || seq_number == seq_total) {
        printf("successful write of ");
        String_print(&client->writing_filename, false);
        printf(
            " %i/%i of size %zu\n",
            seq_number,
            seq_total,
            file_content_chunk.len
        );
    }
    String_free(&file_content_chunk);
    // for each packet recv send a SUC
    rv = send_sequenced_packet(
        sockfd,
        &client->address,
        "SUC",
        seq_number,
        seq_total,
        StringView_from_cstr("")
    );
    if (rv < 0) {
        return -2;
    }
    /*
    if (UFTP_DEBUG) {
        String_print(&client->writing_filename, false);
        printf(" chunk written %i/%i\n", seq_number, seq_total);
    }
    */
    return rv;
}

int handle_input(
    int sockfd,
    Client* client,
    String* packet_recv,
    StringVector* filenames
)
{
    // exit
    if (strncmp("EXT", packet_recv->data, 3) == 0) {
        return handle_EXT(sockfd, &client->address);
    }
    // ls
    else if (strncmp("LST", packet_recv->data, 3) == 0) {
        return handle_LST(sockfd, &client->address, filenames);
    }
    // get 'filename' from server
    else if (strncmp("GFL", packet_recv->data, 3) == 0) {
        return handle_GFL(sockfd, &client->address, packet_recv, filenames);
    }
    // put 'filename' onto server
    else if (strncmp("PFL", packet_recv->data, 3) == 0) {
        return handle_PFL(sockfd, client, packet_recv, filenames);

    }
    // put file chunk onto server
    else if (strncmp("FLQ", packet_recv->data, 3) == 0) {
        return handle_FLQ(sockfd, client, packet_recv, filenames);
    }
    // unknown command, send error
    else {
        return send_ERR(sockfd, &client->address);
    }
}

int send_file(
    int sockfd,
    const Address* client,
    const String* filename,
    const StringVector* valid_filenames
)
{
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

    int rv;
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
        String contents = String_from_file_chunked(
            filename,
            file_chunk_size,
            i * file_chunk_size
        );
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
        String_print(filename, false);
        printf(" file size %zu\n", file_length);
        printf("chunk count: %zu\n", total_seq);
        printf(
            "payload bytes sent: %lu\n",
            total_sent - (UFTP_SEQ_PROTOCOL_SIZE * total_seq)
        );
    }
    return total_sent;
}

void print_input(Client* client, String* packet)
{
    char client_str_buffer[INET6_ADDRSTRLEN];
    memset(&client_str_buffer, 0, sizeof(client_str_buffer));
    uint16_t portnum = Address_port(&client->address);
    const char* client_address_string = inet_ntop(
        client->address.addr.ss_family,
        Address_in_addr(&client->address),
        client_str_buffer,
        sizeof(client_str_buffer)
    );
    if (packet->len < 64) {
        printf("%s:%u(%zu):\n", client_address_string, portnum, packet->len);
        String_dbprint_hex(packet);
    }
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
