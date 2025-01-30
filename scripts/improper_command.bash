#!/bin/bash
# relative paths are specified from the uftp dir!

TEST_NAME='improper command'
TEST_CMD='ext'

TERM_CLR='\033[0m'
TERM_RED='\033[0;31m'
TERM_GRN='\033[0;32m'

echo -e -n "${TERM_CLR}"

# test
echo -n "${TEST_CMD}" | ./uftp_client 127.0.0.1 5000 > /dev/null 2>&1
CLIENT_EXT=$?
if [ $CLIENT_EXT == 1 ]; then
    echo -e "${TEST_NAME}: ${TERM_GRN}pass${TERM_CLR}"
else
    echo -e "${TEST_NAME}: ${TERM_RED}fail${TERM_CLR}"
    echo -e "\tclient returnd ${CLIENT_EXT}, suposed to fail"
fi
