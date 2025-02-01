#ifndef UFTP_HEADER
#define UFTP_HEADER

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "String.h"

#define UFTP_DEBUG 1

#define UFTP_HEADER_SIZE 3
#define UFTP_BUFFER_SIZE 1024

#define UFTP_TIMEOUT_MS 1000

#define UFTP_SEQ_PROTOCOL_SIZE (UFTP_HEADER_SIZE + 3 * sizeof(uint32_t))

// functions for packets
#define CLIEXT 512
#define CLILS 513

typedef struct {
    uint32_t packet_length;
    uint16_t version;
    uint32_t sequence_number;
    uint32_t sequence_total;
    uint16_t function;
} PacketHeader;

typedef struct {
    struct sockaddr_storage addr;
    socklen_t addrlen;
} Address;

typedef struct {
    int fd;
    Address address;
} UdpBoundSocket;

struct sockaddr* Address_sockaddr(const Address* a);
void* Address_in_addr(const Address* a);
uint16_t Address_port(const Address* a);


UdpBoundSocket get_udp_socket(const char* addr, const char* port);

int send_packet(int sockfd, const Address* to, const StringView packet);
int recv_packet(int sockfd, Address* from, String* packet_o, bool append);

int send_sequenced_packet(
    int sockfd,
    const Address* to,
    const char header[UFTP_HEADER_SIZE],
    uint32_t seq_number,
    uint32_t seq_total,
    const StringView payload
);
int parse_sequenced_packet(
    const String* packet,
    char header_o[UFTP_HEADER_SIZE],
    uint32_t* seq_number_o,
    uint32_t* seq_total_o,
    String* payload_o
);

int get_shell_cmd_cout(const char* cmd, String* cout_o);

#endif
