#!/bin/bash
mkdir -p serv
mkdir -p clie 

rm -f serv/*.test
rm -f clie/*.test

./test_file_generator serv/512b.serv 512
./test_file_generator serv/1kb.serv 1024
./test_file_generator serv/1mb.serv 1048576
./test_file_generator serv/100mb.serv 104857600

./test_file_generator clie/512b.clie 512
./test_file_generator clie/1kb.clie 1024
./test_file_generator clie/1mb.clie 1048576
./test_file_generator clie/100mb.clie 104857600
