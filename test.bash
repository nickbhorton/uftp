#!/bin/bash
echo -en "\033[0m"
make clean
make

./scripts/gen_test.bash
echo "test files generated in serv/ and clie/"

cd serv
../server 5000 1> ../logs/server.stdout 2> ../logs/server.stderr &
SERVER_PID=$!
cd ..

sleep 0.25

cd clie
echo "client output:"
../client 127.0.0.1 5000 < ../test.txt
cd ..

kill -INT $SERVER_PID
./scripts/validate_test.bash
