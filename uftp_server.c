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
#include <sys/types.h>

#include "String.h"
#include "StringVector.h"
#include "uftp.h"

void useage();
int validate_port(int argc, char** argv);

int server_loop(UdpBoundSocket* bs, StringVector* filenames);

void print_input(Address* client, String* packet);

int handle_input(
    int sockfd,
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

int send_file(int sockfd, Address* client, String* filename)
{
    String contents = String_from_file(filename);
    int bytes_sent_or_error = send_sequenced_packet(
        sockfd,
        client,
        "FGQ",
        1,
        1,
        StringView_create(&contents, 0, contents.len)
    );
    String_free(&contents);
    return bytes_sent_or_error;
}

int handle_input(
    int sockfd,
    Address* client,
    String* packet_recv,
    StringVector* filenames
)
{
    int bytes_sent_or_error;
    // exit
    if (strncmp("EXT", packet_recv->data, 3) == 0) {
        bytes_sent_or_error =
            send_packet(sockfd, client, StringView_from_cstr("GDB"));
        if (bytes_sent_or_error < 0) {
            return bytes_sent_or_error;
        }
    }
    // ls
    else if (strncmp("LST", packet_recv->data, 3) == 0) {
        for (size_t i = 0; i < filenames->len; i++) {
            int bytes_sent_or_error = send_sequenced_packet(
                sockfd,
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
            if (bytes_sent_or_error < 0) {
                return bytes_sent_or_error;
            }
        }

    }
    // get 'filename'
    else if (strncmp("GFL", packet_recv->data, 3) == 0) {
        // TODO: handle sequenced packet and actaully send the right file
        String filename = String_from_cstr("smnt/files/basic.txt");
        bytes_sent_or_error = send_file(sockfd, client, &filename);
        return bytes_sent_or_error;
    }
    // unknown command, send error
    else {
        bytes_sent_or_error =
            send_packet(sockfd, client, StringView_from_cstr("CER"));
        if (bytes_sent_or_error < 0) {
            return bytes_sent_or_error;
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
    int bytes_recv;
    while (1) {
        memset(&client, 0, sizeof(client));
        client.addrlen = sizeof(client.addrlen);

        int num_events = poll(pfds, 1, 2500);
        if (num_events != 0) {
            if (pfds[0].revents & POLLIN) {
                String packet_in;
                bytes_recv = recv_packet(pfds[0].fd, &client, &packet_in);
                if (bytes_recv >= 0) {
                    Address* current_client =
                        get_client(&cl, &client.addr, client.addrlen);
                    print_input(current_client, &packet_in);
                    if (handle_input(
                            bs->fd,
                            current_client,
                            &packet_in,
                            filenames
                        ) < 0) {
                        String_free(&packet_in);
                        client_list_free(&cl);
                        return -1;
                    }
                    String_free(&packet_in);
                } else {
                    client_list_free(&cl);
                    return -1;
                }
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
    printf("%s:%u(%zu)\n", client_address_string, portnum, packet->len);
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
