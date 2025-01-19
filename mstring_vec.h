#ifndef UFTP_MSTRING_VEC
#define UFTP_MSTRING_VEC

#include "mstring.h"

typedef struct mstring_vec {
    size_t len;
    size_t cap;
    mstring_t* data;
} mstring_vec_t;

void mstring_vec_init(mstring_vec_t* vs);
void mstring_vec_free(mstring_vec_t* s);
void mstring_vec_move(mstring_vec_t* s, mstring_t* mstr);
void mstring_vec_dbprint(mstring_vec_t* s);

#endif
