#include "mstring.h"
#include "mstring_vec.h"

int main()
{
    mstring_vec_t vs;
    mstring_vec_init(&vs);

    mstring_t ts;
    for (size_t i = 0; i < 10; i++) {
        mstring_init(&ts);
        for (size_t j = 0; j < i + 1; j++) {
            mstring_append(&ts, "a");
        }
        mstring_vec_move(&vs, &ts);
    }
    mstring_vec_dbprint(&vs);
    mstring_vec_free(&vs);
}
