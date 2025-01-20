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

// directly from beej.us with no changes
void* get_in_addr(struct sockaddr* sa);

int get_udp_socket(const char* addr, const char* port);

#endif
