#include "String.h"
#include <stdint.h>
#include <stdio.h>

#define INIT_MALLOC 64

// implementation secific

void String_appendn(String* s, char* str, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        String_push_back(s, str[i]);
    }
}

// for now if loc is greater than the end just add to end
void String_insertn(String* s, char* str, size_t loc, size_t size)
{
    if ((int)loc > ((int)s->len) - 1) {
        String_appendn(s, str, size);
        return;
    }
    if (s->data == NULL) {
        s->data = malloc(INIT_MALLOC);
        s->cap = INIT_MALLOC;
        s->len = 0;
    } else {
        while (s->cap < s->len + size) {
            s->cap = s->cap * 2 + 1;
            s->data = realloc(s->data, s->cap);
        }
    }
    for (int i = (int)(s->len + size) - 1; i > (int)(loc + size) - 1; i--) {
        String_set(s, i, String_get(s, i - size));
    }
    for (size_t i = 0; i < size; i++) {
        String_set(s, loc + i, String_get(s, i));
    }
    s->len = s->len + size;
}

// end implementaiton speicifc

void String_push_back(String* s, char c)
{
    if (s->data == NULL) {
        s->data = malloc(INIT_MALLOC);
        s->cap = INIT_MALLOC;
        s->len = 0;
    } else {
        if (s->cap < s->len + 1) {
            s->cap = s->cap * 2 + 1;
            s->data = realloc(s->data, s->cap);
        }
    }
    s->len++;
    String_set(s, s->len - 1, c);
}

String String_new()
{
    String s = {.data = NULL, .len = 0, .cap = 0};
    return s;
}

void String_free(String* s)
{
    free(s->data);
    s->data = NULL;
    s->cap = 0;
    s->len = 0;
}

String String_create(char* cstr, size_t size)
{
    if (INIT_MALLOC >= size) {
        String s =
            {.data = malloc(INIT_MALLOC), .len = size, .cap = INIT_MALLOC};
        memcpy(s.data, cstr, size);
        return s;
    } else {
        String s = {.data = malloc(size), .len = size, .cap = size};
        memcpy(s.data, cstr, size);
        return s;
    }
}

String String_from_cstr(const char* s)
{
    return String_create((char*)s, strlen(s));
}

char* String_to_cstr(String* s)
{
    char* s_cstr = malloc(s->len + 1);
    memset(s_cstr, 0, s->len + 1);
    memcpy(s_cstr, s->data, s->len);
    return s_cstr;
}

char String_get(String* s, size_t index)
{
    if (index < s->len) {
        return s->data[index];
    }
    fprintf(stderr, "String_get out of bound\n");
    return 0;
}

void String_set(String* s, size_t index, char c)
{
    if (index < s->len) {
        s->data[index] = c;
        return;
    }
    fprintf(stderr, "String_get out of bound\n");
}

int String_cmp(String* s1, String* s2)
{
    if (s1->len != s2->len) {
        return -1;
    }
    for (size_t i = 0; i < s1->len; i++) {
        if (String_get(s1, i) != String_get(s2, i)) {
            return i + 1;
        }
    }
    return 0;
}

void String_push_copy(String* to, String* from)
{
    String_insertn(to, from->data, to->len, from->len);
}

void String_push_move(String* to, String from)
{
    String_insertn(to, from.data, to->len, from.len);
    free(from.data);
}

void String_dbprint(String* s)
{
    printf("length: %zu\n", s->len);
    printf("capacity: %zu\n", s->cap);
    for (size_t i = 0; i < s->len; i++) {
        printf("%c", String_get(s, i));
    }
    printf("\n");
}

static void print_nibble_hex(size_t nibble)
{
    static const char* to_hex = "0123456789abcdef";
    printf("%c", to_hex[nibble]);
}

void String_dbprint_hex(String* s)
{
    printf("length: %zu\n", s->len);
    printf("capacity: %zu\n", s->cap);
    size_t bytes_per_line = 8;
    for (size_t i = 0; i < s->len; i++) {
        if (i % bytes_per_line == 0) {
            printf("%06zx\t", i / bytes_per_line);
        }
        char c = String_get(s, i);
        char upper = (c & 0xF0) >> 4;
        print_nibble_hex(upper);
        char lower = (c & 0x0F);
        print_nibble_hex(lower);
        printf(" ");
        if (i % bytes_per_line == bytes_per_line - 1) {
            printf("\n");
        }
    }
    printf("\n");
}

void String_print(String* s)
{
    for (size_t i = 0; i < s->len; i++) {
        printf("%c", String_get(s, i));
    }
    printf("\n");
}

String String_from_file(String* filename)
{
    char* filename_cstr = String_to_cstr(filename);
    FILE* fptr;
    fptr = fopen(filename_cstr, "r");
    char in;
    String s = String_new();
    if (fptr == NULL) {
        fprintf(stderr, "failed to open file %s\n", filename_cstr);
        free(filename_cstr);
        return s;
    }
    free(filename_cstr);
    while ((in = (char)fgetc(fptr)) != EOF) {
        String_push_back(&s, in);
    }
    fclose(fptr);
    return s;
}

void String_to_file(String* s, String* filename)
{
    char* filename_cstr = String_to_cstr(filename);
    FILE* fptr;
    fptr = fopen(filename_cstr, "w");
    if (fptr == NULL) {
        fprintf(stderr, "failed to open file %s\n", filename_cstr);
        free(filename_cstr);
        return;
    }
    free(filename_cstr);
    for (size_t i = 0; i < s->len; i++) {
        fputc(String_get(s, i), fptr);
    }
    fclose(fptr);
}

void String_push_u32(String* s, uint32_t num)
{
    String serialized_int = String_create((char*)&num, sizeof(uint32_t));
    String_push_move(s, serialized_int);
}
void String_push_i32(String* s, int32_t num)
{
    String serialized_int = String_create((char*)&num, sizeof(int32_t));
    String_push_move(s, serialized_int);
}
void String_push_u16(String* s, uint16_t num)
{
    String serialized_int = String_create((char*)&num, sizeof(uint16_t));
    String_push_move(s, serialized_int);
}
void String_push_i16(String* s, int16_t num)
{
    String serialized_int = String_create((char*)&num, sizeof(int16_t));
    String_push_move(s, serialized_int);
}

uint32_t String_parse_u32(String* s, size_t loc)
{
    if (loc + sizeof(uint32_t) > s->len) {
        printf("String_parse string not long enough for parse\n");
        return 0;
    }
    return *((uint32_t*)&s->data[loc]);
}
uint16_t String_parse_u16(String* s, size_t loc)
{
    if (loc + sizeof(uint16_t) > s->len) {
        printf("String_parse string not long enough for parse\n");
        return 0;
    }
    return *((uint16_t*)&s->data[loc]);
}
int32_t String_parse_i32(String* s, size_t loc)
{
    if (loc + sizeof(int32_t) > s->len) {
        printf("String_parse string not long enough for parse\n");
        return 0;
    }
    return *((int32_t*)&s->data[loc]);
}
int16_t String_parse_i16(String* s, size_t loc)
{
    if (loc + sizeof(int16_t) > s->len) {
        printf("String_parse string not long enough for parse\n");
        return 0;
    }
    return *((int16_t*)&s->data[loc]);
}
