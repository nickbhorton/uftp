CC=gcc
CFLAGS=-g3 -Wall -Werror -Wpedantic

all: fileio_test mstring_test mstring_vec_test uftp_client uftp_server

fileio_test: fileio_test.c fileio.o mstring.o mstring_vec.o

mstring_test: mstring_test.c mstring.o

mstring_vec_test: mstring_vec_test.c mstring.o mstring_vec.o

uftp_client: uftp_client.c

uftp_server: uftp_server.c fileio.o mstring.o mstring_vec.o

clean:
	rm uftp_server
	rm uftp_client
	rm mstring_test 
	rm *.o
