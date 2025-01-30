#!/bin/bash
# relative paths are specified from the uftp dir!

source ./scripts/common.bash

TEST_NAME='improper command'
TEST_CMD='ext'

# test
echo -n "${TEST_CMD}" | ./uftp_client 127.0.0.1 5000 > /dev/null 2>&1
CLIENT_EXT=$?
if [ $CLIENT_EXT == 1 ]; then
    echo -e "${TEST_NAME}: ${TERM_GRN}pass${TERM_CLR}"
else
    echo -e "${TEST_NAME}: ${TERM_RED}fail${TERM_CLR}"
    echo -e "\tclient returnd ${CLIENT_EXT}, suposed to fail"
    debug_print_client
fi
