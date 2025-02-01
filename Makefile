CC=gcc
CFLAGS=-g3 -Wall -Werror -fsanitize=address

all: uftp_server uftp_client test_file_generator String_test StringVector_test logs

# snd version 
snd: uftp2_test

uftp2_test: testing.c uftp2.o debug_macros.o String.o
	$(CC) -o $@ $^ $(CFLAGS)

uftp2.o: uftp2.c uftp2.h
# end snd verison

uftp_server_extras.o: uftp_server.h

String.o: String.h

StringVector.o : StringVector.h

uftp.o: uftp.h

uftp_server.o: uftp_server.h

String_test: String_test.c String.o

StringVector_test: StringVector_test.c String.o StringVector.o

uftp_client: uftp_client.c uftp.o String.o StringVector.o debug_macros.o

uftp_server: uftp_server.o String.o StringVector.o uftp_server_extras.o uftp.o debug_macros.o
	$(CC) -o $@ $^ $(CFLAGS)

test_file_generator: test_file_generator.c

serv/test1.serv:
	./scripts/gen_tests.bash

logs:
	mkdir -p logs

test_String: String_test 
	@echo -e "\x1b[33mString tests:\x1b[0m"
	@./String_test

test_StringVector: StringVector_test
	@echo -e "\x1b[33mStringVector tests:\x1b[0m"
	@./StringVector_test

clean:
	rm -f uftp_server
	rm -f uftp_client
	rm -f String_test 
	rm -f StringVector_test
	rm -f uftp2_test 
	rm -f test_file_generator
	rm -rf serv
	rm -rf clie 
	rm -rf logs
	rm -f *.o

test: all serv/test1.serv test_String test_StringVector
	@echo -e "\x1b[33mIntegration tests:\x1b[0m"
	@./scripts/test.bash


.PHONY: clean test all test_String test_StringVector snd
