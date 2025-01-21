#ifndef UFTP_SERVER
#define UFTP_SERVER

#include <unistd.h>

#include <sys/socket.h>

#include "uftp.h"

typedef struct {
    size_t len;
    size_t cap;
    Address** clients;
} client_list_t;

void client_list_init(client_list_t* client_list);
void client_list_free(client_list_t* client_list);

// takes client_info from a recvfrom and either
// 1. matches an existing client in the client_list and returns a pointer said
// client
// 2. creates a new client in the client_list and return a pointer to said
// client
Address* get_client(
    client_list_t* client_list,
    struct sockaddr_storage* client_addr,
    const socklen_t client_addr_len
);

#endif
