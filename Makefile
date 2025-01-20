CC=gcc
CFLAGS=-g3 -Wall -Werror -std=c99 -fsanitize=address

all: test uftp_server

test: String_test StringVector_test
	@echo -e "\x1b[33mString test:\x1b[0m"
	@./String_test
	@echo -e "\x1b[33mStringVector test:\x1b[0m"
	@./StringVector_test

String.o: String.h

StringVector.o : StringVector.h

uftp.o: uftp.h

uftp_server.o: uftp_server.h

String_test: String_test.c String.o

StringVector_test: StringVector_test.c String.o StringVector.o

uftp_client: uftp_client.c uftp.o mstring.o

uftp_server: uftp_server.o String.o StringVector.o uftp_server_extras.o uftp.o


uftp_server_extras.o: uftp_server.h

clean:
	rm -f uftp_server
	rm uftp_client
	rm String_test 
	rm StringVector_test
	rm *.o

.PHONY: clean test
