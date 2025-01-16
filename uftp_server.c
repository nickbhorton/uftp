#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

void useage()
{
    printf("usage: uftp_server port\n  port: the port you want the server to "
           "listen on\n");
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        useage();
        return 1;
    }

    unsigned short int port;
    if (sscanf(argv[1], "%hu", &port) != 1) {
        printf("port given was not a valid integer\n");
        return 1;
    }
    if (port < 5000) {
        printf("port given to small, give a port greater than 5000\n");
        return 1;
    }

    struct addrinfo hints, *res;
    int sfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    int ret;
    ret = getaddrinfo(NULL, argv[1], &hints, &res);
    if (ret != 0) {
        printf("getaddrinfo() error: %s\n", gai_strerror(ret));
        return 1;
    }
    sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sfd < 0) {
        int sock_err = errno;
        printf("socket() error: %s\n", strerror(sock_err));
        return sock_err;
    }

    ret = bind(sfd, res->ai_addr, res->ai_addrlen);
    if (ret < 0) {
        int bind_err = errno;
        printf("bind() error: %s\n", strerror(bind_err));
        return bind_err;
    }

    char msg_buffer[256];
    memset(msg_buffer, 0, sizeof(msg_buffer));

    struct sockaddr_storage from;
    socklen_t from_len;
    while (1) {
        memset(msg_buffer, 0, sizeof(msg_buffer));
        memset(&from, 0, sizeof(from));
        from_len = sizeof(from);
        ret = recvfrom(
            sfd,
            msg_buffer,
            sizeof(msg_buffer),
            0,
            (struct sockaddr*)&from,
            &from_len
        );

        if (ret >= 0) {
            if (from.ss_family == AF_INET) {
                printf("ipv4,");
                printf(
                    "%s,",
                    inet_ntoa(((struct sockaddr_in*)&from)->sin_addr)
                );
            } else if (from.ss_family == AF_INET6) {
                printf("ipv6,");
            }
            printf("%i: ", ret);
            printf("%s\n", msg_buffer);
        } else {
            int recvfrom_err = errno;
            printf("recvfrom() error: %s\n", strerror(recvfrom_err));
            return recvfrom_err;
        }
    }
    return 0;
}
