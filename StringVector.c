#include <stdio.h>

#include "String.h"
#include "StringVector.h"

StringVector StringVector_new()
{
    StringVector sv = {.data = NULL, .len = 0, .cap = 0};
    return sv;
}
void StringVector_free(StringVector* sv)
{
    for (size_t i = 0; i < sv->len; i++) {
        String_free(&sv->data[i]);
    }
    free(sv->data);
}

void StringVector_dbprint(StringVector* sv)
{
    printf("length: %zu\n", sv->len);
    printf("capacity: %zu\n", sv->cap);
    for (size_t i = 0; i < sv->len; i++) {
        printf("%zu:\n", i);
        String_dbprint(&sv->data[i]);
    }
}

String* StringVector_get(StringVector* sv, size_t index)
{
    if (index < sv->len) {
        return &sv->data[index];
    }
    fprintf(stderr, "StringVector_get out of bound\n");
    return 0;
}

void StringVector_push_back_move(StringVector* sv, String s)
{
    if (sv->len == 0) {
        sv->data = malloc(sizeof(String));
        sv->cap = 1;
        sv->len = 0;
    } else {
        if (sv->cap < sv->len + 1) {
            sv->cap = sv->cap * 2 + 1;
            sv->data = realloc(sv->data, sv->cap * sizeof(String));
        }
    }
    sv->len++;
    sv->data[sv->len - 1] = String_new();
    String_push_move(&sv->data[sv->len - 1], s);
}

void StringVector_push_back_copy(StringVector* sv, String* s)
{
    if (sv->len == 0) {
        sv->data = malloc(sizeof(String));
        sv->cap = 1;
        sv->len = 0;
    } else {
        if (sv->cap < sv->len + 1) {
            sv->cap = sv->cap * 2 + 1;
            sv->data = realloc(sv->data, sv->cap * sizeof(String));
        }
    }
    sv->len++;
    sv->data[sv->len - 1] = String_new();
    String_push_copy(&sv->data[sv->len - 1], s);
}

StringVector StringVector_from_split(String* s, char split_on)
{
    char in;
    StringVector sv = StringVector_new();
    for (size_t i = 0; i < s->len; i++) {
        String to_append = String_new();
        while ((in = String_get(s, i)) != split_on && i < s->len) {
            String_push_back(&to_append, in);
            i++;
        }
        StringVector_push_back_move(&sv, to_append);
    }
    return sv;
}
