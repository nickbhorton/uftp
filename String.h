#ifndef UFTP_STRING
#define UFTP_STRING

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} String;

String String_new();
String String_create(char* arr, size_t size);

void String_push_back(String* s, char c);

// bounds checked access
char String_get(String* s, size_t index);
void String_set(String* s, size_t index, char c);

void String_free(String* s);

void String_push_copy(String* to, String* from);
void String_push_move(String* to, String from);

// Serialization stuff
void String_push_u32(String* s, uint32_t num);
void String_push_u16(String* s, uint16_t num);
void String_push_i32(String* s, int32_t num);
void String_push_i16(String* s, int16_t num);

uint32_t String_parse_u32(String* s, size_t loc);
uint16_t String_parse_u16(String* s, size_t loc);
int32_t String_parse_i32(String* s, size_t loc);
int16_t String_parse_i16(String* s, size_t loc);

// if 0 strings are equal
// if -1 strings are not of equal length
// if positive, index of first non equal
int String_cmp(String* s1, String* s2);
int String_cmp_cstr(String* s1, const char* cstring);
int String_cmpn_cstr(String* s1, const char* cstring, size_t n);

void String_print(String* s, bool with_newline);
void String_dbprint(String* s);
void String_dbprint_hex(String* s);

// Read entire file into a String
String String_from_file(String* filename);
// Write entire String to file
void String_to_file(String* s, String* filename);

// These functions use strlen() an should only be used with c string constants
String String_from_cstr(const char* s);

// free responsiblity is given to the caller
char* String_to_cstr(String* s);

#endif
