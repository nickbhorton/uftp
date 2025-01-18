#include <errno.h>
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

void useage();
int validate_port(int argc, char** argv);

void clear_terminal();

void* get_in_addr(struct sockaddr* sa);

int get_udp_server_socket(const char* addr, const char* port);
int server_loop(int sockfd);
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
    char* msg
);

char filenames[5][64] = {
    "my_homework1.txt",
    "cat.png",
    "cool_terminal_app.c",
    "stanford_dragon.obj",
    "nvim.zip"
};

int main(int argc, char** argv)
{
    int sockfd;
    if ((sockfd = get_udp_server_socket(NULL, argv[1])) < 0) {
        return 1;
    }

    if (server_loop(sockfd) < 0) {
        close(sockfd);
    }
    return 0;
}

int handle_input(
    int sockfd,
    int bytes_recv,
    const struct sockaddr_storage* client_addr,
    socklen_t client_addr_len,
    char* msg
)
{
    if (bytes_recv == 3) {
        // exit
        if (strncmp("EXT", msg, 3) == 0) {
            const char* outmsg = "GDB";
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
                return -1;
            }

        }
        // ls
        else if (strncmp("LST", msg, 3) == 0) {
            const char* outcmd = "FLN";
            printf("%lu\n", sizeof(filenames[0]));

            for (size_t i = 0; i < sizeof(filenames) / sizeof(filenames[0]);
                 i++) {
                size_t filename_len =
                    strnlen(filenames[i], sizeof(filenames[0]));
                size_t tosend_size = strlen(outcmd) + filename_len;
                char* tosend = malloc(tosend_size);
                strncpy(tosend, outcmd, 3);
                strncpy(tosend + 3, filenames[i], filename_len);
                if (sendto(
                        sockfd,
                        tosend,
                        tosend_size,
                        0,
                        (const struct sockaddr*)client_addr,
                        client_addr_len
                    ) < 0) {
                    int sendto_err = errno;
                    fprintf(
                        stderr,
                        "sendto() error: %s\n",
                        strerror(sendto_err)
                    );
                    free(tosend);
                    return -1;
                }
                free(tosend);
            }
        } else {
            const char* outmsg = "CER";
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
                return -1;
            }
        }
    } else {
        const char* outmsg = "LER";
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
            return -1;
        }
    }
    return 0;
}

int server_loop(int sockfd)
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
                            msg_buffer
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
