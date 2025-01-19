CC=gcc
CFLAGS=-g3 -Wall -Wpedantic

all: fileio_test mstring_test mstring_vec_test uftp_client uftp_server

fileio_test: fileio_test.c fileio.o mstring.o mstring_vec.o

mstring_test: mstring_test.c mstring.o

mstring_vec_test: mstring_vec_test.c mstring.o mstring_vec.o

uftp_client: uftp_client.c uftp.o mstring.o

uftp_server: uftp_server.c uftp_server.h fileio.o mstring.o mstring_vec.o uftp_server_extras.o uftp.o

uftp.o: uftp.h

mstring.o: mstring.h

uftp_server_extras.o: uftp_server.h

clean:
	rm uftp_server
	rm uftp_client
	rm mstring_test 
	rm *.o
