#ifndef UFTP_MSTRING_VEC
#define UFTP_MSTRING_VEC

#include "String.h"

typedef struct {
    String* data;
    size_t len;
    size_t cap;
} StringVector;

// bounds checked access
String* StringVector_get(const StringVector* s, size_t index);

StringVector StringVector_new();
void StringVector_free(StringVector* sv);
void StringVector_dbprint(const StringVector* sv);

void StringVector_push_back_move(StringVector* sv, String s);
void StringVector_push_back_copy(StringVector* sv, const String* s);

StringVector StringVector_from_split(const String* s, char split_on);

#endif
