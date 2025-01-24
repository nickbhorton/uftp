#include "uftp_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int debug = 1;

int Address_cmp(
    const Address* address,
    const struct sockaddr_storage* c2_addr,
    const socklen_t c2_addr_len
)
{
    if (address->addrlen != c2_addr_len) {
        return -1;
    }
    for (size_t i = 0; i < address->addrlen; i++) {
        if (!(((char*)&address->addr)[i] == ((char*)c2_addr)[i])) {
            return i + 1;
        }
    }
    return 0;
}

void client_list_init(client_list_t* client_list)
{
    memset(client_list, 0, sizeof(client_list_t));
}

void client_list_free(client_list_t* client_list)
{
    for (size_t i = 0; i < client_list->len; i++) {
        free(client_list->clients[i]);
    }
    client_list->len = 0;
    client_list->cap = 0;
    free(client_list->clients);
}

Address* get_client(
    client_list_t* client_list,
    struct sockaddr_storage* client_addr,
    const socklen_t client_addr_len
)
{
    for (size_t ci = 0; ci < client_list->len; ci++) {
        if (Address_cmp(
                client_list->clients[ci],
                client_addr,
                client_addr_len
            ) == 0) {
            if (debug) {
                printf("found client\n");
            }
            return client_list->clients[ci];
        }
    }

    // make sure there is enough memory for a new client
    if (client_list->cap == 0) {
        unsigned int starting_cap = 8;
        client_list->clients = malloc(sizeof(Address*) * starting_cap);
        client_list->cap = starting_cap;
    } else {
        while (client_list->cap < client_list->len + 1) {
            client_list->cap = client_list->cap * 2;
            client_list->clients = realloc(
                client_list->clients,
                sizeof(Address*) * client_list->cap
            );
        }
    }
    // allocate, clear and fill
    client_list->clients[client_list->len] = (Address*)malloc(sizeof(Address));
    memset(client_list->clients[client_list->len], 0, sizeof(Address));
    client_list->clients[client_list->len]->addr = *client_addr;
    client_list->clients[client_list->len]->addrlen = client_addr_len;

    Address* client_to_return = client_list->clients[client_list->len];

    // incremement length
    client_list->len++;
    if (debug) {
        printf(
            "new client created, client_list length: %zu, client_list "
            "capacity: %zu\n",
            client_list->len,
            client_list->cap
        );
    }
    return client_to_return;
}
