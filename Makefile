CC=gcc
CFLAGS=-g3 -Wall -Werror -fsanitize=address

all: server client test_file_generator String_test StringVector_test logs uftp2_test

# string stuff
String.o: String.h
StringVector.o : StringVector.h
String_test: String_test.c String.o
StringVector_test: StringVector_test.c String.o StringVector.o

uftp2_test: testing.c uftp2.c debug_macros.c String.o
	$(CC) -o $@ $^ $(CFLAGS) -DTESTING

uftp2.o: uftp2.c uftp2.h

server: uftp_server2.c uftp2.o debug_macros.o String.o
	$(CC) -o $@ $^ $(CFLAGS)

client: uftp_client2.c uftp2.o debug_macros.o String.o
	$(CC) -o $@ $^ $(CFLAGS)

test_file_generator: test_file_generator.c

logs:
	mkdir -p logs

test_String: String_test 
	@echo -e "\x1b[33mString tests:\x1b[0m"
	@./String_test

test_StringVector: StringVector_test
	@echo -e "\x1b[33mStringVector tests:\x1b[0m"
	@./StringVector_test

clean:
	rm -f server
	rm -f client
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
