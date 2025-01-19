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

#include "fileio.h"
#include "mstring.h"
#include "uftp.h"

void useage();
int validate_port(int argc, char** argv);

int server_loop(int sockfd, mstring_vec_t* filenames);

void print_input(
    int bytes_recv,
    struct sockaddr_storage* client_addr,
    char* msg
);
int handle_input(
    int sockfd,
    client_info_t* client,
    int bytes_recv,
    const char* msg,
    const mstring_vec_t* filenames
);

int main(int argc, char** argv)
{
    if (validate_port(argc, argv) < 0) {
        return 1;
    }

    mstring_vec_t filenames;
    mstring_t ifname;
    mstring_init(&ifname);
    mstring_append(&ifname, "smnt/filenames.txt");
    read_file_lines(&filenames, &ifname);
    mstring_free(&ifname);

    int sockfd;
    if ((sockfd = get_udp_socket(NULL, argv[1])) < 0) {
        mstring_vec_free(&filenames);
        return 1;
    }

    if (server_loop(sockfd, &filenames) < 0) {
        close(sockfd);
    }
    mstring_vec_free(&filenames);
    return 0;
}

int send_rawcmd_packet(int sockfd, client_info_t* client, const char* outcmd)
{
    int bytes_sent;
    if ((bytes_sent = sendto(
             sockfd,
             outcmd,
             strlen(outcmd),
             0,
             (const struct sockaddr*)&client->addr,
             client->addr_len
         )) < 0) {
        int sendto_err = errno;
        fprintf(stderr, "sendto() error: %s\n", strerror(sendto_err));
        return -1;
    }
    return 0;
}

int send_s_packet(
    int sockfd,
    client_info_t* client,
    uint32_t count,
    const char* outcmd
)
{
    mstring_t packet;
    mstring_init(&packet);
    uint32_t packet_filename_count_net = htonl(count);
    mstring_append(&packet, outcmd);
    mstring_appendn(
        &packet,
        (char*)&packet_filename_count_net,
        sizeof(packet_filename_count_net)
    );
    int bytes_sent;
    if ((bytes_sent = sendto(
             sockfd,
             packet.data,
             packet.len,
             0,
             (const struct sockaddr*)&client->addr,
             client->addr_len
         )) < 0) {
        int sendto_err = errno;
        fprintf(stderr, "sendto() error: %s\n", strerror(sendto_err));
        mstring_free(&packet);
        return -1;
    }
    assert(bytes_sent == packet.len);
    mstring_free(&packet);
    return 0;
}

int send_n_packet(
    int sockfd,
    client_info_t* client,
    const mstring_t* line,
    uint32_t seq,
    const char* outcmd
)
{
    mstring_t packet;
    mstring_init(&packet);

    // 2 uint32_t for length and seq
    uint32_t packet_length_net =
        htonl(line->len + strlen(outcmd) + sizeof(uint32_t) * 2);
    uint32_t packet_seq_net = htonl(seq);
    mstring_append(&packet, outcmd);
    mstring_appendn(
        &packet,
        (char*)&packet_length_net,
        sizeof(packet_length_net)
    );
    mstring_appendn(&packet, (char*)&packet_seq_net, sizeof(packet_length_net));
    mstring_appendn(&packet, line->data, line->len);
    int bytes_sent;
    if ((bytes_sent = sendto(
             sockfd,
             packet.data,
             packet.len,
             0,
             (const struct sockaddr*)&client->addr,
             client->addr_len
         )) < 0) {
        int sendto_err = errno;
        fprintf(stderr, "sendto() error: %s\n", strerror(sendto_err));
        mstring_free(&packet);
        return -1;
    }
    assert(bytes_sent == packet.len);
    mstring_free(&packet);
    return 0;
}

int send_file(
    int sockfd,
    client_info_t* client,
    int bytes_recv,
    const char* msg,
    const mstring_vec_t* filenames
)
{
    if (bytes_recv < 3 + sizeof(uint32_t)) {
        fprintf(
            stderr,
            "get 'filename' command packet two small to contain a filename "
            "length\n"
        );
        return -1;
    }
    uint32_t recv_packet_length = ntohl(*((uint32_t*)(msg + 3)));
    if (bytes_recv < recv_packet_length) {
        fprintf(stderr, "GFL packet was too short\n");
        return -1;
    }
    mstring_t get_filename;
    mstring_init(&get_filename);
    mstring_appendn(
        &get_filename,
        (char*)msg + 3 + sizeof(uint32_t),
        recv_packet_length - (3 + sizeof(uint32_t))
    );
    bool found_file = false;
    for (size_t i = 0; i < filenames->len; i++) {
        if (mstring_cmp(&filenames->data[i], &get_filename) == 0) {
            found_file = true;
        }
    }
    if (!found_file) {
        int ret = send_rawcmd_packet(sockfd, client, "FNO");
        if (ret < 0) {
            mstring_free(&get_filename);
            return ret;
        }
    } else {
        mstring_insert(&get_filename, "smnt/files/", 0);
        mstring_vec_t file_lines;
        mstring_vec_init(&file_lines);
        read_file_lines(&file_lines, &get_filename);

        int ret;
        ret = send_s_packet(sockfd, client, file_lines.len, "FGS");
        if (ret < 0) {
            mstring_vec_free(&file_lines);
            mstring_free(&get_filename);
            return ret;
        }
        for (size_t i = 0; i < file_lines.len; i++) {
            ret = send_n_packet(
                sockfd,
                client,
                &file_lines.data[i],
                i + 1,
                "GFN"
            );
            if (ret < 0) {
                mstring_vec_free(&file_lines);
                mstring_free(&get_filename);
                return ret;
            }
        }
        mstring_vec_free(&file_lines);
    }
    mstring_free(&get_filename);
    return 0;
}

int handle_input(
    int sockfd,
    client_info_t* client,
    int bytes_recv,
    const char* msg,
    const mstring_vec_t* filenames
)
{
    int ret;
    // exit
    if (strncmp("EXT", msg, 3) == 0) {
        ret = send_rawcmd_packet(sockfd, client, "GDB");
        if (ret < 0) {
            return ret;
        }
    }
    // ls
    else if (strncmp("LST", msg, 3) == 0) {
        ret = send_s_packet(sockfd, client, filenames->len, "FLS");
        if (ret < 0) {
            return ret;
        }
        for (size_t i = 0; i < filenames->len; i++) {
            ret = send_n_packet(
                sockfd,
                client,
                &filenames->data[i],
                i + 1,
                "FLN"
            );
            if (ret < 0) {
                return ret;
            }
        }

    }
    // get 'filename'
    else if (strncmp("GFL", msg, 3) == 0) {
        ret = send_file(sockfd, client, bytes_recv, msg, filenames);
        if (ret < 0) {
            return ret;
        }
    }
    // unknown command, send error
    else {
        ret = send_rawcmd_packet(sockfd, client, "CER");
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}

int server_loop(int sockfd, mstring_vec_t* filenames)
{
    struct pollfd pfds[1];
    pfds[0].fd = sockfd;
    pfds[0].events = POLLIN;

    client_list_t cl;
    client_list_init(&cl);

    char msg_buffer[128];
    memset(msg_buffer, 0, sizeof(msg_buffer));

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
                    msg_buffer,
                    sizeof(msg_buffer),
                    0,
                    (struct sockaddr*)&client_addr,
                    &client_addr_len
                );

                if (bytes_recv >= 0) {
                    client_info_t* current_client =
                        get_client(&cl, &client_addr, client_addr_len);
                    print_input(bytes_recv, &client_addr, msg_buffer);
                    if (handle_input(
                            sockfd,
                            current_client,
                            bytes_recv,
                            msg_buffer,
                            filenames
                        ) < 0) {
                        client_list_free(&cl);
                        return -1;
                    }
                    memset(msg_buffer, 0, bytes_recv);
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
}

void print_input(
    int bytes_recv,
    struct sockaddr_storage* client_addr,
    char* msg
)
{
    char client_addr_str[INET6_ADDRSTRLEN];
    memset(&client_addr_str, 0, sizeof(client_addr_str));
    printf(
        "%s",
        inet_ntop(
            client_addr->ss_family,
            get_in_addr((struct sockaddr*)client_addr),
            client_addr_str,
            sizeof(client_addr_str)
        )
    );
    if (client_addr->ss_family == AF_INET) {
        printf(":%u", ntohs(((struct sockaddr_in*)client_addr)->sin_port));
    } else if (client_addr->ss_family == AF_INET6) {
        printf(":%u", ntohs(((struct sockaddr_in6*)client_addr)->sin6_port));
    }

    printf("(%i): ", bytes_recv);
    printf("%s\n", msg);
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
