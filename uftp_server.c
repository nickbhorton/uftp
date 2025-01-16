#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

void useage();
int validate_port(int argc, char** argv);

void clear_terminal();

int bind_server_socket_any(char* port_arg, int* sockfd_o);

int server_loop(int sockfd);

int main(int argc, char** argv)
{

    if (validate_port(argc, argv) < 0) {
        return 1;
    }

    int sockfd;
    if (bind_server_socket_any(argv[1], &sockfd) < 0) {
        return 1;
    }

    clear_terminal();

    if (server_loop(sockfd) < 0) {
        shutdown(sockfd, 2);
    }
    return 0;
}

int server_loop(int sockfd)
{
    char msg_buffer[256];
    memset(msg_buffer, 0, sizeof(msg_buffer));

    struct sockaddr_storage from;
    socklen_t from_len;
    int bytes_recv;
    while (1) {
        memset(msg_buffer, 0, sizeof(msg_buffer));
        memset(&from, 0, sizeof(from));
        from_len = sizeof(from);

        bytes_recv = recvfrom(
            sockfd,
            msg_buffer,
            sizeof(msg_buffer),
            0,
            (struct sockaddr*)&from,
            &from_len
        );

        if (bytes_recv >= 0) {
            if (from.ss_family == AF_INET) {
                printf("%s", inet_ntoa(((struct sockaddr_in*)&from)->sin_addr));
                printf(":%u", ntohs(((struct sockaddr_in*)&from)->sin_port));
            } else if (from.ss_family == AF_INET6) {
                printf("error: found AF_INET6 in sockaddr_storage\n");
                return -1;
            }

            printf("(%i): ", bytes_recv);
            printf("%s\n", msg_buffer);
        } else {
            int recvfrom_err = errno;
            printf("recvfrom() error: %s\n", strerror(recvfrom_err));
            return -1;
        }
    }
}

int bind_server_socket_any(char* port_str, int* sockfd_o)
{
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    int ret;
    ret = getaddrinfo(NULL, port_str, &hints, &res);
    if (ret < 0) {
        printf("getaddrinfo() error: %s\n", gai_strerror(ret));
        return -1;
    }

    int sfd;
    sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sfd < 0) {
        int sock_err = errno;
        printf("socket() error: %s\n", strerror(sock_err));
        return -1;
    }

    ret = bind(sfd, res->ai_addr, res->ai_addrlen);
    if (ret < 0) {
        int bind_err = errno;
        printf("bind() error: %s\n", strerror(bind_err));
        return -1;
    }

    *sockfd_o = sfd;
    return 0;
}

int validate_port(int argc, char** argv)
{
    if (argc != 2) {
        useage();
        return -1;
    }
    unsigned short int port;
    if (sscanf(argv[1], "%hu", &port) != 1) {
        printf("port given was not a valid integer\n");
        return -1;
    }
    if (port < 5000) {
        printf("port given to small, give a port greater than 5000\n");
        return -1;
    }
    return 0;
}

void useage()
{
    printf("usage: uftp_server port\n  port: the port you want the server to "
           "listen on\n");
}

void clear_terminal()
{
    printf("\033[2J");   // clear terminal
    printf("\033[1;1H"); // cursor to top left
    fflush(stdout);      // flush these bytes before we do anthing
}
