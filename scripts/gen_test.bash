#!/bin/bash
rm -f *.test
./test_file_generator 512b.test 512
./test_file_generator 1kb.test 1024
./test_file_generator 1mb.test 1048576
./test_file_generator 100mb.test 104857600
