#include "debug_macros.h"
#include "uftp2.h"
#include <stdlib.h>
#include <string.h>

#define TESTING_TIMEOUT_DFLT 1000;
#define TESTING_DBPRINT_DFLT 1;

int UFTP_TIMEOUT_MS = TESTING_TIMEOUT_DFLT;
int UFTP_DEBUG = TESTING_DBPRINT_DFLT;

void restore_default()
{
    UFTP_TIMEOUT_MS = TESTING_TIMEOUT_DFLT;
    UFTP_DEBUG = TESTING_DBPRINT_DFLT;
}

int main()
{
    const char* server_address_cstr = "127.0.0.1";
    const char* server_port_cstr = "5000";
    UdpBoundSocket sfd = get_udp_socket(server_address_cstr, server_port_cstr);
    UdpBoundSocket cfd = get_udp_socket(NULL, "0");
    if (sfd.fd < 0 || cfd.fd < 0) {
        fprintf(stderr, "testing.c failed to setup sockets and addresses\n");
        return 1;
    }

    char payload_buffer[UFTP_PAYLOAD_MAX_SIZE];

    printf("\x1b[0m");

    const char* success = "\x1b[32mpass\x1b[0m";
    const char* fail = "\x1b[31mfail\x1b[0m";

    {
        const char* test_name = "send_packet/recv_packet happy case";
        const char* message = "hello!";
        size_t message_size = strlen(message);
        StringView message_sv = {.data = message, .len = message_size};
        PacketHeaderSend send_head =
            {.sequence_number = 1, .sequence_total = 1, .function = 1234};
        int rv = send_packet(cfd.fd, &sfd.address, send_head, message_sv);
        if (rv < 0) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tsend_packet error\n");
        }

        PacketHeader recv_head = {};
        rv = recv_packet(sfd.fd, &cfd.address, &recv_head, payload_buffer);
        if (rv < 0) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\trecv_packet error\n");
        } else if (strncmp(payload_buffer, message, message_size) != 0) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tmessages did not match\n");
        } else if (send_head.sequence_number != recv_head.sequence_number) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tsequence_number did not match\n");
        } else if (send_head.sequence_total != recv_head.sequence_total) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tsequence_total did not match\n");
        } else if (send_head.function != recv_head.function) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tfunction did not match\n");
        } else if (recv_head.packet_length !=
                   sizeof(PacketHeader) + message_size) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tpacket size was not correct\n");
        } else {
            printf("%s: ", test_name);
            printf("%s\n", success);
        }
        memset(payload_buffer, 0, UFTP_PAYLOAD_MAX_SIZE);
    }

    {
        const char* test_name = "send_packet/recv_packet no payload";
        size_t message_size = 0;
        StringView message_sv = {.data = NULL, .len = message_size};
        PacketHeaderSend send_head =
            {.sequence_number = 1, .sequence_total = 1, .function = 65422};
        int rv = send_packet(cfd.fd, &sfd.address, send_head, message_sv);
        if (rv < 0) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tsend_packet error\n");
        }

        PacketHeader recv_head = {};
        rv = recv_packet(sfd.fd, &cfd.address, &recv_head, payload_buffer);
        if (rv < 0) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\trecv_packet error\n");
        } else if (strncmp(payload_buffer, "\0", 1) != 0) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tpayload buffer was written to\n");
        } else if (send_head.sequence_number != recv_head.sequence_number) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tsequence_number did not match\n");
        } else if (send_head.sequence_total != recv_head.sequence_total) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tsequence_total did not match\n");
        } else if (send_head.function != recv_head.function) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tfunction did not match\n");
        } else if (recv_head.packet_length !=
                   sizeof(PacketHeader) + message_size) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tpacket size was not correct\n");
        } else {
            printf("%s: ", test_name);
            printf("%s\n", success);
        }
        memset(payload_buffer, 0, UFTP_PAYLOAD_MAX_SIZE);
    }

    {
        const char* test_name = "recv_packet when sent in byte by byte";
        const char* message = "helllllllo";
        size_t message_size = strlen(message);
        PacketHeader send_head = {
            .packet_length = htonl(sizeof(PacketHeader) + message_size),
            .version = htonl(UFTP_PROTOCOL_VERSION),
            .sequence_number = htonl(1),
            .sequence_total = htonl(1),
            .function = htonl(65422)
        };
        int rv = 0;
        for (size_t i = 0; i < sizeof(PacketHeader); i++) {
            rv = sendto(
                cfd.fd,
                ((char*)&send_head) + i,
                1,
                0,
                Address_sockaddr(&sfd.address),
                sfd.address.addrlen
            );
            if (rv < 0) {
                printf("%s: ", test_name);
                printf("%s\n", fail);
                printf("\tsend_packet error\n");
                break;
            }
        }
        for (size_t i = 0; i < message_size; i++) {
            rv = sendto(
                cfd.fd,
                message + i,
                1,
                0,
                Address_sockaddr(&sfd.address),
                sfd.address.addrlen
            );
            if (rv < 0) {
                printf("%s: ", test_name);
                printf("%s\n", fail);
                printf("\tsend_packet error\n");
                break;
            }
        }

        PacketHeader recv_head = {};
        rv = recv_packet(sfd.fd, &cfd.address, &recv_head, payload_buffer);
        if (rv < 0) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\trecv_packet error\n");
        } else if (strncmp(payload_buffer, message, message_size) != 0) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tpayload did not match message\n");
        } else if (ntohl(send_head.sequence_number) !=
                   recv_head.sequence_number) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf(
                "\tsequence_number did not match %i %i\n",
                send_head.sequence_number,
                recv_head.sequence_number
            );
        } else if (ntohl(send_head.sequence_total) !=
                   recv_head.sequence_total) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tsequence_total did not match\n");
        } else if (ntohl(send_head.function) != recv_head.function) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tfunction did not match\n");
        } else if (recv_head.packet_length !=
                   sizeof(PacketHeader) + message_size) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tpacket size was not correct\n");
        } else {
            printf("%s: ", test_name);
            printf("%s\n", success);
        }
        memset(payload_buffer, 0, UFTP_PAYLOAD_MAX_SIZE);
    }

    {
        const char* test_name = "recv_packet not enough to parse size";
        const char* message = "nah";
        int rv = sendto(
            cfd.fd,
            message,
            3,
            0,
            Address_sockaddr(&sfd.address),
            sfd.address.addrlen
        );

        PacketHeader recv_head = {};

        UFTP_DEBUG = 0;
        UFTP_TIMEOUT_MS = 5;
        rv = recv_packet(sfd.fd, &cfd.address, &recv_head, payload_buffer);
        restore_default();

        if (rv < 0) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\trecv_packet error\n");
        } else if (rv == 0) {
            printf("%s: ", test_name);
            printf("%s\n", success);
        } else if (rv > 0) {
            printf("%s: ", test_name);
            printf("%s\n", success);
            printf("\trecv_packet did not timeout\n");
        }
        memset(payload_buffer, 0, UFTP_PAYLOAD_MAX_SIZE);
    }

    {
        const char* test_name = "recv_packet message too long";
        char message[UFTP_PAYLOAD_MAX_SIZE + 1];
        memset(message, 1, UFTP_PAYLOAD_MAX_SIZE + 1);
        size_t message_size = UFTP_PAYLOAD_MAX_SIZE + 1;
        StringView message_sv = {.data = message, .len = message_size};
        PacketHeaderSend send_head =
            {.sequence_number = 1, .sequence_total = 1, .function = 1234};

        UFTP_DEBUG = 0;
        int rv = send_packet(cfd.fd, &sfd.address, send_head, message_sv);
        restore_default();

        if (rv < 0) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tsend_packet error\n");
        }

        PacketHeader recv_head = {};

        UFTP_DEBUG = 0;
        rv = recv_packet(sfd.fd, &cfd.address, &recv_head, payload_buffer);
        restore_default();

        if (rv < 0) {
            printf("%s: ", test_name);
            printf("%s\n", success);
        } else {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\trecv_packet did not return -1 returned %i\n", rv);
        }
    }

    {
        const char* test_name = "recv_packet length wrong byte ordering";
        uint32_t length = 4;
        int rv = sendto(
            cfd.fd,
            ((char*)&length),
            4,
            0,
            Address_sockaddr(&sfd.address),
            sfd.address.addrlen
        );

        if (rv < 0) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tsendto error\n");
        }

        PacketHeader recv_head = {};

        UFTP_DEBUG = 0;
        rv = recv_packet(sfd.fd, &cfd.address, &recv_head, payload_buffer);
        restore_default();

        if (rv < 0) {
            printf("%s: ", test_name);
            printf("%s\n", success);
        } else {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\trecv_packet did not return -1\n");
        }
    }

    {
        const char* test_name = "recv_packet did not finish sending";
        uint32_t length = htonl(80);
        int rv = sendto(
            cfd.fd,
            ((char*)&length),
            sizeof(uint32_t),
            0,
            Address_sockaddr(&sfd.address),
            sfd.address.addrlen
        );
        if (rv < 0) {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\tsendto error\n");
        }

        PacketHeader recv_head = {};

        UFTP_TIMEOUT_MS = 5;
        UFTP_DEBUG = 0;
        rv = recv_packet(sfd.fd, &cfd.address, &recv_head, payload_buffer);
        restore_default();

        if (rv == 0) {
            printf("%s: ", test_name);
            printf("%s\n", success);
        } else {
            printf("%s: ", test_name);
            printf("%s\n", fail);
            printf("\trecv_packet did not return -1\n");
        }
    }

    close(sfd.fd);
    close(cfd.fd);
}
