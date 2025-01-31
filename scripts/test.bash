#!/bin/bash
# relative paths are specified from the uftp dir!

source ./scripts/common.bash

echo -e -n "${TERM_CLR}"

# mount server in serv and put in background
cd serv
../uftp_server 5000 1> ../logs/serv.cout 2> ../logs/serv.cerr &
SERVER_PID=$!
cd ..

# to ensure the sever is set up wait for a bit
# sleep 0.1
ps -e | grep uftp_server > /dev/null

./scripts/improper_command.bash
./scripts/nonterm_command.bash
./scripts/exit.bash
./scripts/ls.bash
./scripts/put1.bash

# kill the server for cleanup
kill -INT $SERVER_PID
KILL_EXT=$?
if [ $KILL_EXT != 0 ]; then
    echo "kill server failed for $SERVER_PID"
fi
