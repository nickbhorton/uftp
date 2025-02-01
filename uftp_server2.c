#include <signal.h>

#include "String.h"
#include "debug_macros.h"
#include "uftp2.h"

void useage();
int validate_port(int argc, char** argv);
int set_signals();

static UdpBoundSocket bs;

int main(int argc, char** argv)
{
    if (validate_port(argc, argv) < 0) {
        return 1;
    }
    bs = get_udp_socket(NULL, argv[1]);
    if (bs.fd < 0) {
        return 1;
    }
    if (set_signals() < 0) {
        return 1;
    }

    // setup state
    static char payload_buffer[UFTP_PAYLOAD_MAX_SIZE];
    memset(payload_buffer, 0, UFTP_PAYLOAD_MAX_SIZE);
    static PacketHeader packet_header = {};
    static Address client_address;

    int rv;
    while (true) {
        rv =
            recv_packet(bs.fd, &client_address, &packet_header, payload_buffer);
        if (rv < 0) {
            send_func_only(bs.fd, &client_address, SERV_ERR);
            continue;
        } else if (rv == 0) {
            continue;
        }
        int payload_bytes_recv = rv - (int)sizeof(PacketHeader);

        switch (packet_header.function) {
        case CLIE_LS: {
            String ls_cout;
            rv = get_shell_cmd_cout("ls", &ls_cout);
            if (rv < 0) {
                send_func_only(bs.fd, &client_address, SERV_ERR);
            } else {
                // send full packets
                uint32_t sequence_total =
                    ls_cout.len / UFTP_PAYLOAD_MAX_SIZE + 1;
                for (size_t i = 0; i < sequence_total - 1; i++) {
                    StringView sv = StringView_create(
                        &ls_cout,
                        i * UFTP_PAYLOAD_MAX_SIZE,
                        UFTP_PAYLOAD_MAX_SIZE
                    );
                    send_seq(
                        bs.fd,
                        &client_address,
                        SERV_SUC,
                        i + 1,
                        sequence_total,
                        sv
                    );
                }
                // send partial
                StringView sv = StringView_create(
                    &ls_cout,
                    (sequence_total - 1) * UFTP_PAYLOAD_MAX_SIZE,
                    ls_cout.len % UFTP_PAYLOAD_MAX_SIZE
                );
                send_seq(
                    bs.fd,
                    &client_address,
                    SERV_SUC,
                    sequence_total,
                    sequence_total,
                    sv
                );
            }
            String_free(&ls_cout);
            break;
        }
        case CLIE_EXT:
            rv = send_func_only(bs.fd, &client_address, SERV_SUC);
            break;
        default:
            rv = send_func_only(bs.fd, &client_address, SERV_UNKNOWN_FUNC);
            break;
        }
        if (rv < 0) {
            UFTP_DEBUG_ERR(
                "send error in response to %i",
                packet_header.function
            );
        }

        // reset buffers
        memset(&packet_header, 0, sizeof(PacketHeader));
        if (payload_bytes_recv > 0) {
            memset(payload_buffer, 0, (size_t)payload_bytes_recv);
        }
    }
}

void term_handle(int signum)
{
    if (signum == SIGINT) {
        UFTP_DEBUG_ERR("SIGINT\n");
    } else if (signum == SIGTERM) {
        UFTP_DEBUG_ERR("SIGTERM\n");
    }
    fflush(stdout);
    fflush(stderr);
    close(bs.fd);
    exit(1);
}

int set_signals()
{
    if (signal(SIGINT, &term_handle) == SIG_ERR) {
        fprintf(stderr, "signal for termination failed to be set\n");
        return -1;
    }
    if (signal(SIGTERM, &term_handle) == SIG_ERR) {
        fprintf(stderr, "signal for termination failed to be set\n");
        return -1;
    }
    return 0;
}

int validate_port(int argc, char** argv)
{
    if (argc != 2) {
        useage();
        return -1;
    }
    unsigned short int port;
    if (sscanf(argv[1], "%hu", &port) != 1) {
        fprintf(stderr, "port given was not a valid integer\n");
        return -1;
    }
    if (port < 5000) {
        fprintf(stderr, "port given to small, give a port greater than 5000\n");
        return -1;
    }
    return 0;
}

void useage()
{
    fprintf(
        stderr,
        "usage: uftp_server port\n  port: the port you want the server to "
        "listen on\n"
    );
}
