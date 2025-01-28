#include "uftp.h"
#include "uftp_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        String_free(&client_list->clients[i]->writing_filename);
        free(client_list->clients[i]);
    }
    client_list->len = 0;
    client_list->cap = 0;
    free(client_list->clients);
}

Client* get_client(
    client_list_t* client_list,
    struct sockaddr_storage* client_addr,
    const socklen_t client_addr_len
)
{
    for (size_t ci = 0; ci < client_list->len; ci++) {
        if (Address_cmp(
                &client_list->clients[ci]->address,
                client_addr,
                client_addr_len
            ) == 0) {
            return client_list->clients[ci];
        }
    }

    // make sure there is enough memory for a new client
    if (client_list->cap == 0) {
        unsigned int starting_cap = 8;
        client_list->clients = malloc(sizeof(Client*) * starting_cap);
        client_list->cap = starting_cap;
    } else {
        while (client_list->cap < client_list->len + 1) {
            client_list->cap = client_list->cap * 2;
            client_list->clients = realloc(
                client_list->clients,
                sizeof(Client*) * client_list->cap
            );
        }
    }
    // allocate, clear and fill
    client_list->clients[client_list->len] = (Client*)malloc(sizeof(Client));
    memset(client_list->clients[client_list->len], 0, sizeof(Client));
    client_list->clients[client_list->len]->address.addr = *client_addr;
    client_list->clients[client_list->len]->address.addrlen = client_addr_len;

    Client* client_to_return = client_list->clients[client_list->len];

    // incremement length
    client_list->len++;
    if (UFTP_DEBUG) {
        printf(
            "new client created, client_list length: %zu, client_list "
            "capacity: %zu\n",
            client_list->len,
            client_list->cap
        );
    }
    return client_to_return;
}
