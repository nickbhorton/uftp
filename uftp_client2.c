#include "String.h"
#include "debug_macros.h"
#include "uftp2.h"
#include <bits/pthreadtypes.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void useage();
int validate_address(int argc, char** argv);
int set_signals();
void term_handle(int signum);

UdpBoundSocket bs;

void client_loop(int sockfd, Address* server_address)
{
    // clears term effects
    printf("\033[0m");
    fflush(stdout);

    char input_buffer[UFTP_BUFFER_SIZE];
    memset(input_buffer, 0, UFTP_BUFFER_SIZE);
    char packet_buffer[UFTP_BUFFER_SIZE];
    memset(packet_buffer, 0, UFTP_BUFFER_SIZE);

    struct pollfd pfds[2];
    pfds[0].events = POLLIN;
    pfds[0].fd = 0;
    pfds[1].events = POLLIN;
    pfds[1].fd = sockfd;
    int event_count = 0;
    int rv = 0;
    bool exit_clie = false;
    while ((event_count = poll(pfds, 2, UFTP_TIMEOUT_MS)) >= 0) {
        if (event_count == 0) {
            if (exit_clie) {
                break;
            }
            continue;
        }
        if (pfds[0].revents & POLLIN) {
            rv = read(pfds[0].fd, input_buffer, UFTP_BUFFER_SIZE);
            if (rv < 0) {
                int en = errno;
                UFTP_DEBUG_ERR("read returned %i errno:%i\n", rv, en);
                continue;
            }
            int input_used = 0;
            int input_total = rv;
            /*
            if (input_total > 0) {
                printf("input_total %i\n", input_total);
            }
            */
            while (input_used < input_total) {
                int next_nl = input_used;
                while (input_buffer[next_nl] != '\n' &&
                       input_buffer[next_nl] != '\0') {
                    next_nl++;
                }
                /*
                printf("to process: ");
                for (int i = input_used; i < next_nl; i++) {
                    printf("%c", input_buffer[i]);
                }
                printf("\n");
                */
                if (strncmp("exit\n", input_buffer + input_used, 5) == 0) {
                    input_used += 5;
                    send_func_only(sockfd, server_address, CLIE_EXT);
                    PacketHeader head_exit = {};
                    rv = recv_packet(
                        sockfd,
                        server_address,
                        &head_exit,
                        packet_buffer
                    );
                    if (rv == 0) {
                        UFTP_DEBUG_MSG("server timeout\n");
                        term_handle(SIGTERM);
                    }
                    if (rv <= 0) {
                        term_handle(SIGTERM);
                    }
                    if (head_exit.function >= SERV_ERR) {
                        UFTP_DEBUG_MSG("server error %i\n", head_exit.function);
                        term_handle(SIGTERM);
                    }
                    term_handle(SIGTERM);
                } else if (strncmp("ls\n", input_buffer + input_used, 3) == 0) {
                    input_used += 3;
                    PacketHeader head_ls = {};
                    send_func_only(sockfd, server_address, CLIE_LS);
                    while (1) {
                        rv = recv_packet(
                            sockfd,
                            server_address,
                            &head_ls,
                            packet_buffer
                        );
                        if (rv == 0) {
                            UFTP_DEBUG_MSG("server timeout\n");
                        }
                        if (rv <= 0) {
                            continue;
                        }
                        if (head_ls.function >= SERV_ERR) {
                            UFTP_DEBUG_MSG(
                                "server error %i\n",
                                head_ls.function
                            );
                            continue;
                        }
                        for (int i = 0; i < rv; i++) {
                            printf("%c", packet_buffer[i]);
                        }
                        // exit cond
                        if (head_ls.sequence_total == head_ls.sequence_number) {
                            break;
                        }
                    }
                } else if (strncmp("delete ", input_buffer + input_used, 7) ==
                           0) {
                    input_used += 7;
                    StringView filename_sv = {
                        .data = input_buffer + input_used,
                        .len = next_nl - input_used
                    };
                    // delete the '\n' so strlen works
                    input_buffer[next_nl] = 0;
                    input_used = next_nl + 1;

                    // set the filename
                    send_seq(
                        sockfd,
                        server_address,
                        CLIE_SET_FN,
                        1,
                        1,
                        filename_sv
                    );
                    PacketHeader delete_head = {};
                    rv = recv_packet(
                        sockfd,
                        server_address,
                        &delete_head,
                        packet_buffer
                    );
                    if (rv == 0) {
                        UFTP_DEBUG_MSG("server timeout\n");
                    }
                    if (rv <= 0) {
                        continue;
                    }
                    if (delete_head.function >= SERV_ERR) {
                        UFTP_DEBUG_MSG(
                            "server error %i\n",
                            delete_head.function
                        );
                        continue;
                    }

                    // do the delete
                    send_func_only(sockfd, server_address, CLIE_DEL);
                    rv = recv_packet(
                        sockfd,
                        server_address,
                        &delete_head,
                        packet_buffer
                    );
                    if (rv == 0) {
                        UFTP_DEBUG_MSG("server timeout\n");
                    }
                    if (rv <= 0) {
                        continue;
                    }
                    if (delete_head.function >= SERV_ERR) {
                        UFTP_DEBUG_MSG(
                            "server error %i\n",
                            delete_head.function
                        );
                        continue;
                    }
                } else if (strncmp("set ", input_buffer + input_used, 4) == 0) {
                    input_used += 4;
                    StringView filename = {
                        .data = input_buffer + input_used,
                        .len = input_used - next_nl
                    };
                    send_seq(
                        sockfd,
                        server_address,
                        CLIE_SET_FN,
                        1,
                        1,
                        filename
                    );
                } else if (strncmp("size\n", input_buffer + input_used, 5) ==
                           0) {
                    input_used += 5;
                    send_func_only(sockfd, server_address, CLIE_GET_FS);
                } else if (strncmp("clear\n", input_buffer + input_used, 6) ==
                           0) {
                    input_used += 6;
                    printf("\033[2J\033[1;1H");
                    fflush(stdout);
                } else if (strncmp("fc\n", input_buffer + input_used, 3) == 0) {
                    input_used += 3;
                    send_empty_seq(sockfd, server_address, CLIE_GET_FC, 0, 1);
                } else if (strncmp("put ", input_buffer + input_used, 4) == 0) {
                    input_used += 4;
                    StringView filename_sv = {
                        .data = input_buffer + input_used,
                        .len = next_nl - input_used
                    };
                    // delete the '\n' so strlen works
                    input_buffer[next_nl] = 0;
                    input_used = next_nl + 1;

                    struct stat sb = {};
                    rv = lstat(filename_sv.data, &sb);
                    if (rv < 0) {
                        printf("bad filename\n");
                        continue;
                    }
                    // set the filename buffer
                    // set the filename
                    send_seq(
                        sockfd,
                        server_address,
                        CLIE_SET_FN,
                        1,
                        1,
                        filename_sv
                    );

                    PacketHeader put_head = {};
                    rv = recv_packet(
                        sockfd,
                        server_address,
                        &put_head,
                        packet_buffer
                    );
                    if (rv == 0) {
                        UFTP_DEBUG_MSG("server timeout\n");
                    }
                    if (rv <= 0) {
                        continue;
                    }
                    if (put_head.function >= SERV_ERR) {
                        UFTP_DEBUG_MSG("server error %i\n", put_head.function);
                        continue;
                    }

                    // allocate the proper buffer
                    send_empty_seq(
                        sockfd,
                        server_address,
                        CLIE_ALLOC_FB,
                        sb.st_size,
                        1
                    );
                    rv = recv_packet(
                        sockfd,
                        server_address,
                        &put_head,
                        packet_buffer
                    );
                    if (rv == 0) {
                        UFTP_DEBUG_MSG("server timeout\n");
                    }
                    if (rv <= 0) {
                        continue;
                    }
                    if (put_head.function >= SERV_ERR) {
                        UFTP_DEBUG_MSG("server error %i\n", put_head.function);
                        continue;
                    }

                    size_t chunk_count =
                        (sb.st_size / UFTP_PAYLOAD_MAX_SIZE) +
                        ((sb.st_size % UFTP_PAYLOAD_MAX_SIZE) ? 1 : 0);
                    for (size_t i = 0; i < chunk_count; i++) {
                        String chunk = String_from_file_chunked(
                            filename_sv,
                            UFTP_PAYLOAD_MAX_SIZE,
                            i * UFTP_PAYLOAD_MAX_SIZE
                        );
                        StringView chunk_sv =
                            StringView_create(&chunk, 0, chunk.len);
                        if (chunk.len != 0) {
                            send_seq(
                                sockfd,
                                server_address,
                                CLIE_PUT_FC,
                                i * UFTP_PAYLOAD_MAX_SIZE,
                                1,
                                chunk_sv
                            );
                            rv = recv_packet(
                                sockfd,
                                server_address,
                                &put_head,
                                packet_buffer
                            );
                            if (rv == 0) {
                                UFTP_DEBUG_MSG("server timeout\n");
                            }
                            if (rv <= 0) {
                                String_free(&chunk);
                                continue;
                            }
                            if (put_head.function >= SERV_ERR) {
                                String_free(&chunk);
                                UFTP_DEBUG_MSG(
                                    "server error %i\n",
                                    put_head.function
                                );
                                continue;
                            }
                        }
                        String_free(&chunk);
                    }
                    // write the file
                    send_empty_seq(
                        sockfd,
                        server_address,
                        CLIE_WRITE_F,
                        sb.st_size,
                        1
                    );
                    rv = recv_packet(
                        sockfd,
                        server_address,
                        &put_head,
                        packet_buffer
                    );
                    if (rv == 0) {
                        UFTP_DEBUG_MSG("server timeout\n");
                    }
                    if (rv <= 0) {
                        continue;
                    }
                    if (put_head.function >= SERV_ERR) {
                        UFTP_DEBUG_MSG("server error %i\n", put_head.function);
                        continue;
                    }
                    UFTP_DEBUG_MSG("send file of size %li\n", sb.st_size);
                } else if (strncmp("get ", input_buffer + input_used, 4) == 0) {
                    input_used += 4;
                    StringView filename_sv = {
                        .data = input_buffer + input_used,
                        .len = next_nl - input_used
                    };
                    // delete the '\n' so strlen works
                    input_buffer[next_nl] = 0;
                    input_used = next_nl + 1;

                    FILE* fptr = fopen(filename_sv.data, "w");
                    if (fptr == NULL) {
                        printf("could not open file of that name\n");
                        continue;
                    }

                    // set the filename buffer
                    send_seq(
                        sockfd,
                        server_address,
                        CLIE_SET_FN,
                        1,
                        1,
                        filename_sv
                    );
                    PacketHeader head_fn = {};
                    rv = recv_packet(
                        sockfd,
                        server_address,
                        &head_fn,
                        packet_buffer
                    );
                    if (rv == 0) {
                        printf("server timeout\n");
                    }
                    if (rv <= 0) {
                        fclose(fptr);
                        continue;
                    }
                    if (head_fn.function >= SERV_ERR) {
                        printf("server error\n");
                        fclose(fptr);
                        continue;
                    }

                    send_empty_seq(sockfd, server_address, CLIE_GET_FS, 1, 1);
                    PacketHeader head_fs = {};
                    rv = recv_packet(
                        sockfd,
                        server_address,
                        &head_fs,
                        packet_buffer
                    );
                    if (rv == 0) {
                        printf("server timeout\n");
                    }
                    if (rv <= 0) {
                        fclose(fptr);
                        continue;
                    }
                    if (head_fs.function >= SERV_ERR) {
                        printf("no such file\n");
                        fclose(fptr);
                        continue;
                    }
                    size_t filesize = head_fs.sequence_number;
                    char* file_buffer = malloc(filesize);
                    size_t chunk_count =
                        (filesize / UFTP_PAYLOAD_MAX_SIZE) +
                        ((filesize % UFTP_PAYLOAD_MAX_SIZE) ? 1 : 0);
                    for (size_t i = 0; i < chunk_count; i++) {
                        rv = send_empty_seq(
                            sockfd,
                            server_address,
                            CLIE_GET_FC,
                            i * UFTP_PAYLOAD_MAX_SIZE,
                            1
                        );
                        rv = recv_packet(
                            sockfd,
                            server_address,
                            &head_fs,
                            packet_buffer
                        );
                        if (rv == 0) {
                            free(file_buffer);
                            printf("server timeout\n");
                        }
                        if (rv <= 0) {
                            free(file_buffer);
                            fclose(fptr);
                            continue;
                        }
                        if (head_fs.function >= SERV_ERR) {
                            printf("file transfer error\n");
                            free(file_buffer);
                            fclose(fptr);
                            continue;
                        }
                        memcpy(
                            file_buffer + (i * UFTP_PAYLOAD_MAX_SIZE),
                            packet_buffer,
                            rv - sizeof(PacketHeader)
                        );
                    }
                    int rv = fwrite(file_buffer, sizeof(char), filesize, fptr);
                    if (rv != filesize) {
                        printf(
                            "only wrote %i bytes to file of size %zu\n",
                            rv,
                            filesize
                        );
                    }
                    UFTP_DEBUG_MSG("wrote file of size %i\n", rv);
                    fclose(fptr);
                    free(file_buffer);
                } else {
                    input_used = next_nl + 1;
                }
                // clear packet buffer
                memset(packet_buffer, 0, UFTP_PAYLOAD_MAX_SIZE);
            }
            memset(input_buffer, 0, UFTP_PAYLOAD_MAX_SIZE);
        }
        if (pfds[1].revents & POLLIN) {
            PacketHeader head = {};
            rv = recv_packet(sockfd, server_address, &head, input_buffer);
            if (rv < 0) {
                continue;
            }
            if (head.function == SERV_SUC) {
                printf("\033[0;32mSERV_SUC ");
            }
            if (head.function == SERV_SUC_LS) {
                printf("\033[0;32mSERV_SUC_LS ");
            }
            if (head.function == SERV_SUC_SET_FN) {
                printf("\033[0;32mSERV_SUC_SET_FN ");
            }
            if (head.function == SERV_SUC_ALLOC_FB) {
                printf("\033[0;32mSERV_SUC_ALLOC_FB ");
            }
            if (head.function == SERV_SUC_PUT_FC) {
                printf("\033[0;32mSERV_SUC_PUT_FC ");
            }
            if (head.function == SERV_SUC_WRITE_F) {
                printf("\033[0;32mSERV_SUC_WRITE_F ");
            }
            if (head.function == SERV_SUC_GET_FS) {
                printf("\033[0;32mSERV_SUC_GET_FS ");
            }
            if (head.function == SERV_SUC_GET_FC) {
                printf("\033[0;32mSERV_SUC_GET_FC ");
            }

            if (head.function == SERV_ERR) {
                printf("\033[0;31mSERV_ERR ");
            }
            if (head.function == SERV_ERR_LS) {
                printf("\033[0;31mSERV_ERR_LS ");
            }
            if (head.function == SERV_ERR_SET_FN) {
                printf("\033[0;31mSERV_ERR_SET_FN ");
            }
            if (head.function == SERV_ERR_ALLOC_FB) {
                printf("\033[0;31mSERV_ERR_ALLOC_FB ");
            }
            if (head.function == SERV_ERR_PUT_FC) {
                printf("\033[0;31mSERV_ERR_PUT_FC ");
            }
            if (head.function == SERV_ERR_WRITE_F) {
                printf("\033[0;31mSERV_ERR_WRITE_F ");
            }
            if (head.function == SERV_ERR_GET_FS) {
                printf("\033[0;31mSERV_ERR_GET_FS ");
            }
            if (head.function == SERV_ERR_GET_FC) {
                printf("\033[0;31mSERV_ERR_GET_FC ");
            }
            printf(
                "%i/%i\033[0m, %lu\n",
                head.sequence_number,
                head.sequence_total,
                head.packet_length - sizeof(PacketHeader)
            );
            for (size_t i = 0; i < head.packet_length - 20; i++) {
                printf("%c", input_buffer[i]);
            }
            fflush(stdout);

            // reset buffers
            memset(input_buffer, 0, rv);
            memset(&head, 0, sizeof(PacketHeader));
        }
    }
}

int main(int argc, char** argv)
{
    int rv = validate_address(argc, argv);
    if (rv < 0) {
        return 1;
    }
    rv = set_signals();
    if (rv < 0) {
        fprintf(stderr, "signals failed to be set\n");
        return 1;
    }
    bs = get_udp_socket(NULL, "0");
    if (bs.fd < 0) {
        return 1;
    }
    Address server_address = {};
    rv = get_address(argv[1], argv[2], &server_address);
    if (rv < 0) {
        return 1;
    }

    client_loop(bs.fd, &server_address);
    close(bs.fd);
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

int validate_address(int argc, char** argv)
{
    int ret;
    if (argc != 3) {
        useage();
        return -1;
    }

    // ensure vaules passed are corrent.
    // TODO: find another way to do this
    struct sockaddr_in sa;
    ret = inet_pton(AF_INET, argv[1], &(sa.sin_addr));
    if (ret == 0) {
        fprintf(stderr, "address given was not a valid network address\n");
        return -1;
    } else if (ret < 0) {
        int inet_pton_err = errno;
        fprintf(
            stderr,
            "address family is not supported, %s\n",
            strerror(inet_pton_err)
        );
        return -1;
    }

    unsigned short int port;
    if (sscanf(argv[2], "%hu", &port) != 1) {
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
        "usage: uftp_client address port\n  address: the uftp servers "
        "address\n  port:    the uftp servers port\n"
    );
}
