#!/bin/bash

# relative paths are specified from the uftp dir!

# mount server in serv and put in background
cd serv
../uftp_server 5000 1>../logs/serv.cout 2>../logs/serv.cerr &
SERVER_PID=$!
cd ..

echo -n "exit" | ./uftp_client 127.0.0.1 5000

# kill the server for cleanup
echo "kill server $SERVER_PID"
kill -9 $SERVER_PID
