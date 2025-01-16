CC=gcc
CFLAGS=-g3 -Wall -Werror -Wpedantic

all: uftp_client uftp_server

uftp_client: uftp_client.c

uftp_server: uftp_server.c

clean:
	rm uftp_server
	rm uftp_client
