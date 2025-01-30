CC=gcc
CFLAGS=-g3 -Wall -Werror -fsanitize=address

all: uftp_server uftp_client test_file_generator

test_file_generator: test_file_generator.c

String.o: String.h

StringVector.o : StringVector.h

uftp.o: uftp.h

uftp_server.o: uftp_server.h

String_test: String_test.c String.o

StringVector_test: StringVector_test.c String.o StringVector.o

uftp_client: uftp_client.c uftp.o String.o StringVector.o debug_macros.o

uftp_server: uftp_server.o String.o StringVector.o uftp_server_extras.o uftp.o debug_macros.o
	$(CC) -o $@ $^ $(CFLAGS)

uftp_server_extras.o: uftp_server.h

clean:
	rm -f uftp_server
	rm -f uftp_client
	rm -f String_test 
	rm -f StringVector_test
	rm -rf serv
	rm -rf clie 
	rm *.o

test: String_test StringVector_test
	@./scripts/gen_tests.bash
	@echo -e "\x1b[33mString test:\x1b[0m"
	@./String_test
	@echo -e "\x1b[33mStringVector test:\x1b[0m"
	@./StringVector_test


.PHONY: clean test
