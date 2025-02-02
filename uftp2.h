#ifndef UFTP_HEADER
#define UFTP_HEADER

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "String.h"

#ifdef TESTING
extern int UFTP_DEBUG;
extern int UFTP_TIMEOUT_MS;
#else
#define UFTP_DEBUG 1
#define UFTP_TIMEOUT_MS 1000
#endif

#define UFTP_BUFFER_SIZE 1024
#define UFTP_PROTOCOL_VERSION 1

#define CLIE_EXT 512
#define CLIE_LS 513
#define CLIE_SET_FN 514
#define CLIE_ALLOC_FB 515
#define CLIE_PUT_FC 516
#define CLIE_WRITE_F 517

#define CLIE_GET_FS 518
#define CLIE_GET_FC 519

#define SERV_SUC 0
#define SERV_SUC_LS 1
#define SERV_SUC_SET_FN 2
#define SERV_SUC_ALLOC_FB 3
#define SERV_SUC_PUT_FC 4
#define SERV_SUC_WRITE_F 5

#define SERV_ERR 256
#define SERV_ERR_LS 257
#define SERV_ERR_SET_FN 258
#define SERV_ERR_ALLOC_FB 259
#define SERV_ERR_PUT_FC 260
#define SERV_ERR_WRITE_F 261

#define SERV_UNKNOWN_FUNC 262

typedef struct __attribute__((packed)) {
    uint32_t packet_length;
    uint32_t version;
    uint32_t sequence_number;
    uint32_t sequence_total;
    uint32_t function;
} PacketHeader;

typedef struct __attribute__((packed)) {
    uint32_t sequence_number;
    uint32_t sequence_total;
    uint32_t function;
} PacketHeaderSend;

#define UFTP_PAYLOAD_MAX_SIZE (UFTP_BUFFER_SIZE - sizeof(PacketHeader))

typedef struct {
    struct sockaddr_storage addr;
    socklen_t addrlen;
} Address;

typedef struct {
    int fd;
    Address address;
} UdpBoundSocket;

// returns:
//   >=0: number of bytes sent
//    <0: error occured
int send_packet(
    int sockfd,
    const Address* to,
    const PacketHeaderSend head,
    const StringView payload
);
int send_func_only(int sockfd, const Address* to, uint32_t function);
int send_empty_seq(
    int sockfd,
    const Address* to,
    uint32_t function,
    uint32_t sequence_number,
    uint32_t sequence_total
);
int send_seq(
    int sockfd,
    const Address* to,
    uint32_t function,
    uint32_t sequence_number,
    uint32_t sequence_total,
    const StringView sv
);

// returns:
//   >=0: number of bytes recv
//    <0: error occured
int recv_packet(
    int sockfd,
    Address* from,
    PacketHeader* header_o,
    char* payload_buffer_o
);

struct sockaddr* Address_sockaddr(const Address* a);
void* Address_in_addr(const Address* a);
uint16_t Address_port(const Address* a);

UdpBoundSocket get_udp_socket(const char* addr, const char* port);
int get_address(const char* address, const char* port, Address* server_address);

int get_shell_cmd_cout(const char* cmd, String* cout_o);

#endif
