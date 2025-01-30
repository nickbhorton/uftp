#!/bin/bash
# relative paths are specified from the uftp dir!

source ./scripts/common.bash

TEST_NAME='client put'
TEST_FILENAME='test1.clie'
TEST_CMD='put clie/'${TEST_FILENAME}';'

# test
echo -n "${TEST_CMD}" | ./uftp_client 127.0.0.1 5000 1> clie.stdout 2> clie.stderr

CLIENT_EXT=$?
if [ $CLIENT_EXT != 0 ]; then
    echo -e "${TEST_NAME}: ${TERM_RED}fail${TERM_CLR} client did not return 0"
    echo -e "\tclient returnd ${CLIENT_EXT}"
    debug_print_client
else
    cmp serv/test1.clie clie/test1.clie -s
    RESULT=$?

    if [ $RESULT == 0 ]; then
        echo -e "${TEST_NAME}: ${TERM_GRN}pass${TERM_CLR}"
    else
        if [ $RESULT == 1 ]; then
            echo -e "${TEST_NAME}: ${TERM_RED}fail${TERM_CLR}"
            echo -n -e "\t"
            cmp clie.stdout expected.out -b
            echo -e "\tclient returnd ${CLIENT_EXT}"
            debug_print_client
        else
            echo -e "${TEST_NAME}: ${TERM_RED}cmp failed${TERM_CLR}, unknown result"
            echo -e "\tclient returnd ${CLIENT_EXT}"
            debug_print_client
        fi
    fi
fi

# clean up
rm -f expected.out
rm -f clie.stdout
rm -f clie.stderr
