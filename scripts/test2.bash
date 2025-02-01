#!/bin/bash

TERM_CLR='\033[0m'
TERM_RED='\033[0;31m'
TERM_GRN='\033[0;32m'

mkdir -p serv
mkdir -p logs 
cd serv
../uftp_server 5000 1> ../logs/serv.cout 2> ../logs/serv.cerr &
SERVER_PID=$!
cd ..

ps -e | grep uftp_server
sleep 2

# cleanup
rm -rf serv
# kill server
kill -INT $SERVER_PID
KILL_EXT=$?
if [ $KILL_EXT != 0 ]; then
    echo "kill server failed for $SERVER_PID"
fi
