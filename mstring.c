#include "mstring.h"

void mstring_init(mstring_t* s)
{
    memset(s, 0, sizeof(*s));
    s->cap = 1;
    s->data = malloc(s->cap);
}

void mstring_free(mstring_t* s)
{
    free(s->data);
    s->data = NULL;
}

void mstring_appendn(mstring_t* s, char* str, size_t size)
{
    while (s->cap < s->len + size) {
        s->cap = s->cap * 2 + 1;
        s->data = realloc(s->data, s->cap);
    }
    memcpy(s->data + s->len, str, size);
    s->len = s->len + size;
}

// sus
void mstring_append(mstring_t* s, const char* str)
{
    size_t size = strlen(str);
    mstring_appendn(s, (char*)str, size);
}

int mstring_cmp(mstring_t* s1, mstring_t* s2)
{
    if (s1->len != s2->len) {
        return -1;
    }
    for (size_t i = 0; i < s1->len; i++) {
        if (s1->data[i] != s2->data[i]) {
            return i + 1;
        }
    }
    return 0;
}

int mstring_cmp_cstr(mstring_t* s1, const char* cstring)
{
    if (s1->len != strlen(cstring)) {
        return -1;
    }
    for (size_t i = 0; i < s1->len; i++) {
        if (s1->data[i] != cstring[i]) {
            return i + 1;
        }
    }
    return 0;
}

void mstring_insertn(mstring_t* s, char* str, size_t loc, size_t size)
{
    if ((int)loc > ((int)s->len) - 1) {
        mstring_appendn(s, str, size);
        return;
    }
    while (s->cap < s->len + size) {
        s->cap = s->cap * 2 + 1;
        s->data = realloc(s->data, s->cap);
    }
    for (int i = (int)(s->len + size) - 1; i > (int)(loc + size) - 1; i--) {
        // printf("%i\n", i);
        s->data[i] = s->data[i - size];
    }
    for (size_t i = 0; i < size; i++) {
        s->data[loc + i] = str[i];
    }
    s->len = s->len + size;
}

void mstring_insert(mstring_t* s, const char* str, size_t loc)
{
    size_t size = strlen(str);
    mstring_insertn(s, (char*)str, loc, size);
}

void mstring_dbprint(mstring_t* s)
{
    printf("length: %zu\n", s->len);
    printf("capacity: %zu\n", s->cap);
    for (size_t i = 0; i < s->len; i++) {
        printf("%c", s->data[i]);
    }
    printf("\n");
}
void mstring_print(mstring_t* s)
{
    for (size_t i = 0; i < s->len; i++) {
        printf("%c", s->data[i]);
    }
}
