#!/bin/bash
# relative paths are specified from the uftp dir!

# mount server in serv and put in background
cd serv
../uftp_server 5000 1> ../logs/serv.cout 2> ../logs/serv.cerr &
SERVER_PID=$!
cd ..

echo "sleep 0.5 second"
sleep 0.5
ps -e | grep uftp_server

./scripts/client_exit.bash

# kill the server for cleanup
echo "kill server $SERVER_PID"
kill -INT $SERVER_PID
