#!/bin/bash
{ echo -n "GFL"; printf "\x0\x0\x0\x17"; echo -n "my_homework1.txx"; } | ncat -u 127.0.0.1 5000
