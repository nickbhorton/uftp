#!/bin/bash
# for common funcitons and globals

TERM_CLR='\033[0m'
TERM_RED='\033[0;31m'
TERM_GRN='\033[0;32m'

debug_print_client() {
    echo -e -n "\tclient.stderr: "
    cat clie.stderr
    if [ "$(stat -c %s clie.stderr)" = "0" ]; then
        echo ""
    fi
    echo -e -n "\tclient.stdout: "
    cat clie.stdout
    if [ "$(stat -c %s clie.stdout)" = "0" ]; then
        echo ""
    fi
}

