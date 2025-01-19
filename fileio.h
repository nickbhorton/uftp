#ifndef UFTP_FILEIO
#define UFTP_FILEIO

#include "mstring_vec.h"

void read_file_lines(mstring_vec_t* vs, mstring_t* filename);
void write_file_lines(mstring_vec_t* vs, mstring_t* filename);

#endif
