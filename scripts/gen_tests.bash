#!/bin/bash

mkdir clie
mkdir serv

./test_file_generator serv/test1.serv 128
./test_file_generator serv/test2.serv 512
./test_file_generator serv/test3.serv 4096
./test_file_generator serv/test4.serv 32768
./test_file_generator serv/test5.serv 8388608

./test_file_generator clie/test1.clie 128
./test_file_generator clie/test2.clie 512
./test_file_generator clie/test3.clie 4096
./test_file_generator clie/test4.clie 32768
./test_file_generator clie/test5.clie 8388608
