#include "mstring_vec.h"
#include "mstring.h"
#include <stdio.h>

void mstring_vec_init(mstring_vec_t* vs)
{
    vs->len = 0;
    vs->cap = 0;
    vs->data = NULL;
}
void mstring_vec_free(mstring_vec_t* s)
{
    for (size_t i = 0; i < s->len; i++) {
        mstring_free(&s->data[i]);
    }
}
void mstring_vec_move(mstring_vec_t* s, mstring_t* mstr)
{
    s->len++;
    while (s->cap < s->len) {
        s->cap = s->cap * 2 + 1;
        if (s->len == 1) {
            s->data = malloc(s->cap * sizeof(mstring_t));
        } else {
            s->data = realloc(s->data, s->cap * sizeof(mstring_t));
        }
    }
    s->data[s->len - 1].len = mstr->len;
    s->data[s->len - 1].cap = mstr->cap;
    s->data[s->len - 1].data = mstr->data;

    mstr->len = 0;
    mstr->cap = 0;
    mstr->data = NULL;
}
void mstring_vec_dbprint(mstring_vec_t* s)
{
    for (size_t i = 0; i < s->len; i++) {
        mstring_dbprint(&s->data[i]);
        printf("\n");
    }
}
