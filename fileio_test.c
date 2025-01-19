#include "fileio.h"
#include "mstring.h"
#include "mstring_vec.h"

int main()
{
    mstring_vec_t vs;
    mstring_t ifname;
    mstring_init(&ifname);
    mstring_append(&ifname, "fileio.c");
    read_file_lines(&vs, &ifname);
    mstring_free(&ifname);

    // mstring_vec_dbprint(&vs);
    mstring_t ofname;
    mstring_init(&ofname);
    mstring_append(&ofname, "test.out");
    write_file_lines(&vs, &ofname);
    mstring_free(&ofname);
    mstring_vec_free(&vs);
}
