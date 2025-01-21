#include "uftp_server.h"
#include <assert.h>
#include <errno.h>
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

void print_input(Address* client, char* msg, int bytes_recv);

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

    String if_name = String_from_cstr("smnt/filenames.txt");
    String if_content = String_from_file(&if_name);
    StringVector filenames = StringVector_from_split(&if_content, '\n');
    String_free(&if_name);
    String_free(&if_content);

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

int send_packet(int sockfd, Address* client, String* packet)
{
    int bytes_sent;
    if ((bytes_sent = sendto(
             sockfd,
             packet->data,
             packet->len,
             0,
             Address_sockaddr(client),
             client->addrlen
         )) < 0) {
        int sendto_err = errno;
        fprintf(stderr, "sendto() error: %s\n", strerror(sendto_err));
        return -1;
    } else if (bytes_sent != packet->len) {
        fprintf(
            stderr,
            "send_packet() error: %s\n",
            "bytes sent were less than packet length"
        );
        return -1;
    }
    return bytes_sent;
}

int send_rawcmd_packet(int sockfd, Address* client, const char* outcmd)
{
    String packet = String_create((char*)outcmd, 3);
    int bytes_sent_or_error = send_packet(sockfd, client, &packet);
    String_free(&packet);
    return bytes_sent_or_error;
}

int send_s_packet(
    int sockfd,
    Address* client,
    uint32_t count,
    const char* outcmd
)
{
    String packet = String_create((char*)outcmd, 3);
    String_push_u32(&packet, htonl(count));
    int bytes_sent_or_error = send_packet(sockfd, client, &packet);
    String_free(&packet);
    return bytes_sent_or_error;
}

int send_n_packet(
    int sockfd,
    Address* client,
    String* line,
    uint32_t seq,
    const char* outcmd
)
{
    String packet = String_create((char*)outcmd, 3);

    // 2 uint32_t for length and seq
    String_push_u32(
        &packet,
        htonl(line->len + strlen(outcmd) + sizeof(uint32_t) * 2)
    );
    String_push_u32(&packet, htonl(seq));
    String_push_copy(&packet, line);

    int bytes_sent_or_error = send_packet(sockfd, client, &packet);
    String_free(&packet);
    return bytes_sent_or_error;
    return 0;
}

int send_file(int sockfd, Address* client, String* filename)
{
    String contents = String_from_file(filename);
    int bytes_sent_or_error = send_s_packet(sockfd, client, 1, "FGS");
    if (bytes_sent_or_error < 0) {
        String_free(&contents);
        return bytes_sent_or_error;
    }
    bytes_sent_or_error = send_n_packet(sockfd, client, &contents, 1, "GFN");
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
        bytes_sent_or_error = send_rawcmd_packet(sockfd, client, "GDB");
        if (bytes_sent_or_error < 0) {
            return bytes_sent_or_error;
        }
    }
    // ls
    else if (strncmp("LST", packet_recv->data, 3) == 0) {
        bytes_sent_or_error =
            send_s_packet(sockfd, client, filenames->len, "FLS");
        if (bytes_sent_or_error < 0) {
            return bytes_sent_or_error;
        }
        for (size_t i = 0; i < filenames->len; i++) {
            bytes_sent_or_error = send_n_packet(
                sockfd,
                client,
                &filenames->data[i],
                i + 1,
                "FLN"
            );
            if (bytes_sent_or_error < 0) {
                return bytes_sent_or_error;
            }
        }

    }
    // get 'filename'
    else if (strncmp("GFL", packet_recv->data, 3) == 0) {
        String filename = String_from_cstr("smnt/files/basic.txt");
        bytes_sent_or_error = send_file(sockfd, client, &filename);
        if (bytes_sent_or_error < 0) {
            return bytes_sent_or_error;
        }
    }
    // unknown command, send error
    else {
        bytes_sent_or_error = send_rawcmd_packet(sockfd, client, "CER");
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

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));

    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;
    int bytes_recv;
    while (1) {
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr_len = sizeof(client_addr);

        int num_events = poll(pfds, 1, 2500);
        if (num_events != 0) {
            if (pfds[0].revents & POLLIN) {
                bytes_recv = recvfrom(
                    pfds[0].fd,
                    buffer,
                    sizeof(buffer),
                    0,
                    (struct sockaddr*)&client_addr,
                    &client_addr_len
                );

                if (bytes_recv >= 0) {
                    Address* current_client =
                        get_client(&cl, &client_addr, client_addr_len);
                    print_input(current_client, buffer, bytes_recv);
                    String packet_recv = String_create(buffer, bytes_recv);
                    if (handle_input(
                            bs->fd,
                            current_client,
                            &packet_recv,
                            filenames
                        ) < 0) {
                        client_list_free(&cl);
                        return -1;
                    }
                    String_free(&packet_recv);
                    memset(buffer, 0, bytes_recv);
                } else {
                    int recvfrom_err = errno;
                    fprintf(
                        stderr,
                        "recvfrom() error: %s\n",
                        strerror(recvfrom_err)
                    );
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

void print_input(Address* client, char* msg, int bytes_recv)
{
    char client_addr_str[INET6_ADDRSTRLEN];
    memset(&client_addr_str, 0, sizeof(client_addr_str));
    printf(
        "%s:%u(%i): %s\n",
        inet_ntop(
            client->addr.ss_family,
            Address_in_addr(client),
            client_addr_str,
            sizeof(client_addr_str)
        ),
        Address_port(client),
        bytes_recv,
        msg
    );
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
