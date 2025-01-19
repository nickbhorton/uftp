#include "fileio.h"
#include "mstring.h"
#include "mstring_vec.h"

void read_file_lines(mstring_vec_t* vs, mstring_t* ifname)
{
    char* filename_cstr = malloc(ifname->len + 1);
    memset(filename_cstr, 0, ifname->len + 1);
    memcpy(filename_cstr, ifname->data, ifname->len);
    mstring_vec_init(vs);
    FILE* fptr;
    fptr = fopen(filename_cstr, "r");
    char in;
    mstring_t s;
    mstring_init(&s);
    while ((in = (char)fgetc(fptr)) != EOF) {
        if (in == '\n') {
            mstring_vec_move(vs, &s);
            mstring_init(&s);
        } else {
            mstring_appendn(&s, &in, 1);
        }
    }
    free(filename_cstr);
    fclose(fptr);
}

void write_file_lines(mstring_vec_t* vs, mstring_t* ofname)
{
    char* filename_cstr = malloc(ofname->len + 1);
    memset(filename_cstr, 0, ofname->len + 1);
    memcpy(filename_cstr, ofname->data, ofname->len);
    FILE* fptr;
    fptr = fopen(filename_cstr, "w");
    for (size_t i = 0; i < vs->len; i++) {
        for (size_t j = 0; j < vs->data[i].len; j++) {
            fputc((int)vs->data[i].data[j], fptr);
        }
        fputc((int)'\n', fptr);
    }
    free(filename_cstr);
    fclose(fptr);
}
