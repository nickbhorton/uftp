#ifndef UFTP_HEADER
#define UFTP_HEADER

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>

#include <sys/socket.h>

#include <netdb.h>

#include <arpa/inet.h>

typedef struct {
    struct sockaddr_storage addr;
    socklen_t addrlen;
} Address;

struct sockaddr* Address_sockaddr(Address* a);
void* Address_in_addr(Address* a);
uint16_t Address_port(Address* a);

typedef struct {
    int fd;
    Address address;
} UdpBoundSocket;

UdpBoundSocket get_udp_socket(const char* addr, const char* port);

#endif
