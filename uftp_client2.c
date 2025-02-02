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

UdpBoundSocket bs;

void client_loop(int sockfd, Address* server_address)
{
    // clears term effects
    printf("\033[0m");

    char buffer[UFTP_BUFFER_SIZE];
    memset(buffer, 0, UFTP_BUFFER_SIZE);
    PacketHeader head = {};

    struct pollfd pfds[2];
    pfds[0].events = POLLIN;
    pfds[0].fd = 0;
    pfds[1].events = POLLIN;
    pfds[1].fd = sockfd;
    int event_count = 0;
    int rv = 0;
    bool exit = false;
    while ((event_count = poll(pfds, 2, UFTP_TIMEOUT_MS)) >= 0) {
        if (event_count == 0) {
            continue;
        }
        if (pfds[0].revents & POLLIN) {
            rv = read(pfds[0].fd, buffer, UFTP_BUFFER_SIZE);
            if (rv < 0) {
                int en = errno;
                UFTP_DEBUG_ERR("read returned %i errno:%i\n", rv, en);
                continue;
            }
            if (strncmp("exit\n", buffer, 5) == 0) {
                send_func_only(sockfd, server_address, CLIE_EXT);
                exit = true;
            } else if (strncmp("ls\n", buffer, 3) == 0) {
                send_func_only(sockfd, server_address, CLIE_LS);
            } else if (strncmp("set ", buffer, 4) == 0) {
                StringView sv = {.data = buffer + 4, .len = rv - 5};
                send_seq(sockfd, server_address, CLIE_SET_FN, 1, 1, sv);
            } else if (strncmp("size\n", buffer, 5) == 0) {
                send_func_only(sockfd, server_address, CLIE_GET_FS);
            } else if (strncmp("clear\n", buffer, 6) == 0) {
                printf("\033[2J\033[1;1H");
                fflush(stdout);
            } else if (strncmp("fc\n", buffer, 6) == 0) {
                send_empty_seq(sockfd, server_address, CLIE_GET_FC, 0, 1);
            } else if (strncmp("put ", buffer, 4) == 0) {
                StringView filename = {.data = buffer + 4, .len = rv - 5};
                struct stat sb = {};
                // delete the '\n' so strlen works
                buffer[rv - 1] = 0;
                rv = lstat(buffer + 4, &sb);
                if (rv < 0) {
                    printf("bad filename\n");
                    continue;
                }
                // set the filename buffer
                StringView filename_sv = {
                    .data = buffer + 4,
                    .len = strlen(buffer + 4)
                };
                send_seq(
                    sockfd,
                    server_address,
                    CLIE_SET_FN,
                    1,
                    1,
                    filename_sv
                );
                // allocate the proper buffer
                send_empty_seq(
                    sockfd,
                    server_address,
                    CLIE_ALLOC_FB,
                    sb.st_size,
                    1
                );

                size_t chunk_count =
                    (sb.st_size / UFTP_PAYLOAD_MAX_SIZE) +
                    ((sb.st_size % UFTP_PAYLOAD_MAX_SIZE) ? 1 : 0);
                for (size_t i = 0; i < chunk_count; i++) {
                    String chunk = String_from_file_chunked(
                        filename,
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
            } else if (strncmp("get ", buffer, 4) == 0) {
                // make strnlen viable
                buffer[rv - 1] = 0;
                FILE* fptr = fopen(buffer + 4, "w");
                if (fptr == NULL) {
                    printf("could not open file of that name\n");
                    continue;
                }

                // set the filename buffer
                StringView filename_sv = {
                    .data = buffer + 4,
                    .len = strlen(buffer + 4)
                };
                send_seq(
                    sockfd,
                    server_address,
                    CLIE_SET_FN,
                    1,
                    1,
                    filename_sv
                );
                PacketHeader head_fn = {};
                rv = recv_packet(sockfd, server_address, &head_fn, buffer);
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
                rv = recv_packet(sockfd, server_address, &head_fs, buffer);
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
                    rv = recv_packet(sockfd, server_address, &head_fs, buffer);
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
                        buffer,
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
            }
            memset(buffer, 0, UFTP_PAYLOAD_MAX_SIZE);
        }
        if (pfds[1].revents & POLLIN) {
            rv = recv_packet(sockfd, server_address, &head, buffer);
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
                printf("%c", buffer[i]);
            }
            fflush(stdout);

            // reset buffers
            memset(buffer, 0, rv);
            memset(&head, 0, sizeof(PacketHeader));

            if (exit) {
                break;
            }
        }
    }
}

int main(int argc, char** argv)
{
    int rv = validate_address(argc, argv);
    if (rv < 0) {
        useage();
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
