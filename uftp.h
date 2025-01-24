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

#define UFTP_SEQ_PROTOCOL_SIZE (3 + 3 * sizeof(uint32_t))

typedef struct {
    struct sockaddr_storage addr;
    socklen_t addrlen;
} Address;

struct sockaddr* Address_sockaddr(Address* a);
uint16_t Address_port(Address* a);

/*
 * from beej returns a pointer to either a addr_sin or addr_sin6 dependand on
 * addr.ss_family
 */
void* Address_in_addr(Address* a);

typedef struct {
    int fd;
    Address address;
} UdpBoundSocket;

UdpBoundSocket get_udp_socket(const char* addr, const char* port);

int send_packet(int sockfd, Address* to, StringView packet);
int recv_packet(int sockfd, Address* from, String* packet_o, bool append);

/*
 * Protocol:
 *   header (char[UFTP_HEADER_SIZE]): indicates what kind of sequence this is.
 *      Why?
 *      1. hexdump type commands
 *      2. multiple types of sequenced data sent out at a time
 *   packet_length (uint32_t): size of payload
 *   seq_number (uint32_t): numbered packet in the sequence
 *   seq_total (uint32_t): total number of packets in the sequence
 *   payload (char[variable]): contents of packet
 */
int send_sequenced_packet(
    int sockfd,
    Address* to,
    char header[UFTP_HEADER_SIZE],
    uint32_t seq_number,
    uint32_t seq_total,
    StringView payload
);
int parse_sequenced_packet(
    const String* packet,
    char header_o[UFTP_HEADER_SIZE],
    uint32_t* seq_number_o,
    uint32_t* seq_total_o,
    String* payload_o
);

/*
 * Potentailly very harmfull command, should not be used based on any user
 * input, the cmd parameter should be directly filled with a c string constant.
 */
int get_shell_cmd_cout(const char* cmd, String* cout_o);

#endif
