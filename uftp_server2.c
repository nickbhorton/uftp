#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    static char filename_buffer[UFTP_PAYLOAD_MAX_SIZE];
    memset(filename_buffer, 0, UFTP_PAYLOAD_MAX_SIZE);
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
        case CLIE_PUT_FC: {
            FILE* fptr = fopen(filename_buffer, "w");
            if (fptr == NULL) {
                rv = send_func_only(bs.fd, &client_address, SERV_BADF);
                break;
            }
            // goto end of file
            rv = fseek(fptr, 0, SEEK_END);
            if (rv < 0) {
                rv = send_func_only(bs.fd, &client_address, SERV_BADP);
                fclose(fptr);
                break;
            }
            int file_endpos = ftell(fptr);
            if (rv < 0) {
                rv = send_func_only(bs.fd, &client_address, SERV_BADP);
                fclose(fptr);
                break;
            }
            UFTP_DEBUG_MSG("file end at %i\n", rv);
            // write zero bytes until the position specified
            while (file_endpos < packet_header.sequence_number) {
                rv = fputc(0, fptr);
                if (rv < 0) {
                    rv = send_func_only(bs.fd, &client_address, SERV_BADP);
                    goto file_put_fail;
                }
            }
            // goto pos specified
            rv = fseek(fptr, packet_header.sequence_number, SEEK_SET);
            if (rv < 0) {
                rv = send_func_only(bs.fd, &client_address, SERV_BADP);
                fclose(fptr);
                break;
            }
            // write bytes from payload
            for (size_t i = 0; i < payload_bytes_recv; i++) {
                rv = fputc(payload_buffer[i], fptr);
                if (rv < 0) {
                    rv = send_func_only(bs.fd, &client_address, SERV_BADP);
                    goto file_put_fail;
                }
            }
            rv = send_empty_seq(
                bs.fd,
                &client_address,
                SERV_SUC,
                packet_header.sequence_number,
                1
            );
        file_put_fail:
            fclose(fptr);
            break;
        }
        case CLIE_GET_FC: {
            FILE* fptr = fopen(filename_buffer, "r");
            if (fptr == NULL) {
                rv = send_func_only(bs.fd, &client_address, SERV_BADF);
                break;
            }
            rv = fseek(fptr, packet_header.sequence_number, SEEK_SET);
            if (rv < 0) {
                rv = send_func_only(bs.fd, &client_address, SERV_BADP);
                fclose(fptr);
                break;
            }
            size_t payload_size = UFTP_PAYLOAD_MAX_SIZE;
            for (size_t i = 0; i < UFTP_PAYLOAD_MAX_SIZE; i++) {
                int c = fgetc(fptr);
                if (c == EOF) {
                    payload_size = i;
                    break;
                }
                payload_buffer[i] = (char)c;
            }
            StringView sv = {.data = payload_buffer, .len = payload_size};
            rv = send_seq(
                bs.fd,
                &client_address,
                SERV_SUC,
                packet_header.sequence_number,
                1,
                sv
            );
            fclose(fptr);
            break;
        }
        case CLIE_GET_FS: {
            struct stat sb = {};
            rv = lstat(filename_buffer, &sb);
            if (rv < 0) {
                rv = send_func_only(bs.fd, &client_address, SERV_BADF);
                break;
            }
            // This means max filesize to send is 2^32 4Gb
            rv =
                send_empty_seq(bs.fd, &client_address, SERV_SUC, sb.st_size, 1);
            break;
        }
        case CLIE_SET_FN: {
            memset(filename_buffer, 0, UFTP_PAYLOAD_MAX_SIZE);
            memcpy(filename_buffer, payload_buffer, payload_bytes_recv);
            UFTP_DEBUG_MSG("filename set: '");
            for (size_t i = 0; i < payload_bytes_recv; i++) {
                fprintf(stdout, "%c", payload_buffer[i]);
            }
            fprintf(stdout, "'\n");
            rv = send_func_only(bs.fd, &client_address, SERV_SUC);
            break;
        }
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
                rv = send_seq(
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
    exit(0);
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
