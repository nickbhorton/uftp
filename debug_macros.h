#ifndef UFTP_DEBUG_MACROS
#define UFTP_DEBUG_MACROS

#include <stdio.h>

void fprinttime(FILE* stream);

#define UFTP_DEBUG_ERR(...)                                                    \
    fprinttime(stderr);                                                        \
    fprintf(stderr, __VA_ARGS__);

#define UFTP_DEBUG_MSG(...)                                                    \
    fprinttime(stdout);                                                        \
    fprintf(stdout, __VA_ARGS__);

#endif
