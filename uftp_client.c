#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

void useage();

int validate_address(int argc, char** argv);

int bind_client_socket(int* sockfd_o);

int main(int argc, char** argv)
{
    if (validate_address(argc, argv) < 0) {
        return 1;
    }
    int sockfd;
    if (bind_client_socket(&sockfd) < 0) {
        return 1;
    }

    struct addrinfo serv_hints, *serv_res;
    memset(&serv_hints, 0, sizeof(serv_hints));
    serv_hints.ai_family = AF_INET;
    serv_hints.ai_socktype = SOCK_DGRAM;

    int ret;
    ret = getaddrinfo(argv[1], argv[2], &serv_hints, &serv_res);
    if (ret != 0) {
        fprintf(stderr, "server getaddrinfo() error: %s\n", gai_strerror(ret));
        return 1;
    }

    const char* msg = "hello server";
    ret = sendto(
        sockfd,
        msg,
        strlen(msg),
        0,
        serv_res->ai_addr,
        serv_res->ai_addrlen
    );
    if (ret < 0) {
        int sendto_err = errno;
        fprintf(stderr, "sendto() error: %s\n", strerror(sendto_err));
        return -1;
    } else {
        printf("sendto() sent: %i bytes\n", ret);
    }

    shutdown(sockfd, 2);
    return 0;
}

int validate_address(int argc, char** argv)
{
    int ret;
    if (argc != 3) {
        useage();
        return 1;
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

int bind_client_socket(int* sockfd_o)
{
    int ret;
    struct addrinfo cli_hints, *cli_res;
    memset(&cli_hints, 0, sizeof(cli_hints));
    cli_hints.ai_family = AF_INET;
    cli_hints.ai_socktype = SOCK_DGRAM;
    cli_hints.ai_flags = AI_PASSIVE;

    ret = getaddrinfo(NULL, "0", &cli_hints, &cli_res);
    if (ret != 0) {
        printf("client getaddrinfo() error: %s\n", gai_strerror(ret));
        return -1;
    }

    int sfd =
        socket(cli_res->ai_family, cli_res->ai_socktype, cli_res->ai_protocol);
    if (sfd < 0) {
        int sock_err = errno;
        printf("socket() error: %s\n", strerror(sock_err));
        return -1;
    }

    // beej says this will get rid of that 'pesky "Address already in use" error
    // message'
    int tru = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &tru, sizeof(tru)) == -1) {
        int setsockopt_err = errno;
        printf("setsockopt() error: %s\n", strerror(setsockopt_err));
        return -1;
    }

    ret = bind(sfd, cli_res->ai_addr, cli_res->ai_addrlen);
    if (ret < 0) {
        int bind_err = errno;
        printf("bind() error: %s\n", strerror(bind_err));
        return -1;
    }

    *sockfd_o = sfd;
    return 0;
}
