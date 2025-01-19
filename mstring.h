#ifndef UFTP_MSTRING
#define UFTP_MSTRING

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mstring {
    size_t len;
    size_t cap;
    char* data;
} mstring_t;

void mstring_init(mstring_t* s);
void mstring_free(mstring_t* s);
void mstring_appendn(mstring_t* s, char* str, size_t size);
void mstring_append(mstring_t* s, const char* str);
// for now if loc is greater than the end just add to end
void mstring_insertn(mstring_t* s, char* str, size_t loc, size_t size);
void mstring_insert(mstring_t* s, const char* str, size_t loc);
// if 0 strings are equal
// if -1 strings are not of equal length
// if positive, index of first non equal
int mstring_cmp(mstring_t* s1, mstring_t* s2);
void mstring_dbprint(mstring_t* s);

#endif
