#include "uftp2.h"

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
    }


    close(sfd.fd);
    close(cfd.fd);
}
