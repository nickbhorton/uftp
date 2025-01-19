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

void useage();
int validate_port(int argc, char** argv);

void clear_terminal();

void* get_in_addr(struct sockaddr* sa);

int get_udp_server_socket(const char* addr, const char* port);
int server_loop(int sockfd, mstring_vec_t* filenames);
void print_input(
    int bytes_recv,
    struct sockaddr_storage* client_addr,
    char* msg
);
int handle_input(
    int sockfd,
    int bytes_recv,
    const struct sockaddr_storage* client_addr,
    socklen_t client_addr_len,
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
    if ((sockfd = get_udp_server_socket(NULL, argv[1])) < 0) {
        mstring_vec_free(&filenames);
        return 1;
    }

    if (server_loop(sockfd, &filenames) < 0) {
        close(sockfd);
    }
    mstring_vec_free(&filenames);
    return 0;
}

int sent_rawcmd_packet(
    int sockfd,
    const struct sockaddr_storage* client_addr,
    socklen_t client_addr_len,
    const char* outcmd
)
{
    int bytes_sent;
    if ((bytes_sent = sendto(
             sockfd,
             outcmd,
             strlen(outcmd),
             0,
             (const struct sockaddr*)client_addr,
             client_addr_len
         )) < 0) {
        int sendto_err = errno;
        fprintf(stderr, "sendto() error: %s\n", strerror(sendto_err));
        return -1;
    }
    return 0;
}

int send_s_packet(
    int sockfd,
    const struct sockaddr_storage* client_addr,
    socklen_t client_addr_len,
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
             (const struct sockaddr*)client_addr,
             client_addr_len
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
    const struct sockaddr_storage* client_addr,
    socklen_t client_addr_len,
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
             (const struct sockaddr*)client_addr,
             client_addr_len
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
    int bytes_recv,
    const struct sockaddr_storage* client_addr,
    socklen_t client_addr_len,
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
    bool found = false;
    for (size_t i = 0; i < filenames->len; i++) {
        if (mstring_cmp(&filenames->data[i], &get_filename) == 0) {
            found = true;
        }
    }
    if (!found) {
        const char* outmsg = "FNO";
        if (sendto(
                sockfd,
                outmsg,
                strlen(outmsg),
                0,
                (const struct sockaddr*)client_addr,
                client_addr_len
            ) < 0) {
            int sendto_err = errno;
            fprintf(stderr, "sendto() error: %s\n", strerror(sendto_err));
            mstring_free(&get_filename);
            return -1;
        }
    } else {
        mstring_insert(&get_filename, "smnt/files/", 0);
        mstring_vec_t file_lines;
        mstring_vec_init(&file_lines);
        read_file_lines(&file_lines, &get_filename);

        int ret;
        ret = send_s_packet(
            sockfd,
            client_addr,
            client_addr_len,
            file_lines.len,
            "FGS"
        );
        if (ret < 0) {
            mstring_vec_free(&file_lines);
            mstring_free(&get_filename);
            return ret;
        }
        for (size_t i = 0; i < file_lines.len; i++) {
            ret = send_n_packet(
                sockfd,
                client_addr,
                client_addr_len,
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
    int bytes_recv,
    const struct sockaddr_storage* client_addr,
    socklen_t client_addr_len,
    const char* msg,
    const mstring_vec_t* filenames
)
{
    int ret;
    // exit
    if (strncmp("EXT", msg, 3) == 0) {
        ret = sent_rawcmd_packet(sockfd, client_addr, client_addr_len, "GDB");
        if (ret < 0) {
            return ret;
        }
    }
    // ls
    else if (strncmp("LST", msg, 3) == 0) {
        ret = send_s_packet(
            sockfd,
            client_addr,
            client_addr_len,
            filenames->len,
            "FLS"
        );
        if (ret < 0) {
            return ret;
        }
        for (size_t i = 0; i < filenames->len; i++) {
            ret = send_n_packet(
                sockfd,
                client_addr,
                client_addr_len,
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
        ret = send_file(
            sockfd,
            bytes_recv,
            client_addr,
            client_addr_len,
            msg,
            filenames
        );
        if (ret < 0) {
            return ret;
        }
    }
    // unknown command, send error
    else {
        ret = sent_rawcmd_packet(sockfd, client_addr, client_addr_len, "CER");
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
                    print_input(bytes_recv, &client_addr, msg_buffer);
                    if (handle_input(
                            sockfd,
                            bytes_recv,
                            &client_addr,
                            client_addr_len,
                            msg_buffer,
                            filenames
                        ) < 0) {
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
                    return -1;
                }
            } else {
                fprintf(stderr, "poll() unecpected event\n");
            }
        }
    }
}

int get_udp_server_socket(const char* addr, const char* port)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // ipv4
    hints.ai_socktype = SOCK_DGRAM;
    if (addr == NULL) {
        hints.ai_flags = AI_PASSIVE;
    }

    struct addrinfo* server_info;
    int ret;
    ret = getaddrinfo(addr, port, &hints, &server_info);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo() error: %s\n", gai_strerror(ret));
        return -1;
    }

    // linked list traversal vibe from beej.us
    int sfd;
    struct addrinfo* ptr;
    for (ptr = server_info; ptr != NULL; ptr = ptr->ai_next) {
        if ((sfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) <
            0) {
            int sock_err = errno;
            fprintf(stderr, "socket() error: %s\n", strerror(sock_err));
            continue; // next loop
        }
        if ((bind(sfd, ptr->ai_addr, ptr->ai_addrlen)) < 0) {
            int sock_err = errno;
            close(sfd);
            char failed_addr[INET6_ADDRSTRLEN];
            memset(&failed_addr, 0, sizeof(failed_addr));
            inet_ntop(
                ptr->ai_family,
                get_in_addr(ptr->ai_addr),
                failed_addr,
                sizeof(failed_addr)
            );
            fprintf(
                stderr,
                "failed to bind() to %s: %s\n",
                strerror(sock_err),
                failed_addr
            );
            continue; // next loop
        }
        break; // if here we have a valid bound SOCK_DGRAM socket
    }

    if (ptr == NULL) {
        fprintf(stderr, "failed to find and bind a socket");
        return -1;
    }

    char success_addr[INET6_ADDRSTRLEN];
    memset(&success_addr, 0, sizeof(success_addr));
    inet_ntop(
        ptr->ai_family,
        get_in_addr(ptr->ai_addr),
        success_addr,
        sizeof(success_addr)
    );
    fprintf(stdout, "server bound to address %s\n", success_addr);

    freeaddrinfo(server_info);

    // setting sockopts
    int yes = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        int sock_err = errno;
        fprintf(stderr, "socket() error: %s\n", strerror(sock_err));
        close(sfd);
        return -1;
    }

    return sfd;
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

void clear_terminal()
{
    printf("\033[2J");   // clear terminal
    printf("\033[1;1H"); // cursor to top left
    fflush(stdout);      // flush these bytes before we do anthing
}

// directly from beej.us with no changes
void* get_in_addr(struct sockaddr* sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
