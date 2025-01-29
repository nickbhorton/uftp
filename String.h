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
String String_create(const char* arr, size_t size);
String String_create_of_size(char fill, size_t size);

void String_push_back(String* s, char c);

// bounds checked access
char String_get(const String* s, size_t index);
void String_set(String* s, size_t index, char c);

void String_free(String* s);

// Works like c++ copy leaves 'from' untouched while move give ownership of
// 'from's data to 'to'.
void String_push_copy(String* to, const String* from);
void String_push_move(String* to, String from);

// Serialization stuff
void String_push_u32(String* s, uint32_t num);
void String_push_u16(String* s, uint16_t num);
void String_push_i32(String* s, int32_t num);
void String_push_i16(String* s, int16_t num);

uint32_t String_parse_u32(const String* s, size_t loc);
uint16_t String_parse_u16(const String* s, size_t loc);
int32_t String_parse_i32(const String* s, size_t loc);
int16_t String_parse_i16(const String* s, size_t loc);

// if 0 strings are equal
// if -1 strings are not of equal length
// if positive, index of first non equal
int String_cmp(const String* s1, const String* s2);
int String_cmp_cstr(const String* s1, const char* cstring);
int String_cmpn_cstr(const String* s1, const char* cstring, size_t n);

void String_print(const String* s, bool with_newline);
void String_printn(const String* s, size_t n, bool with_newline);
void String_print_like_cstr(const String* s, bool with_newline);
void String_dbprint(const String* s);
void String_dbprint_hex(const String* s);

// Read entire file into a String
String String_from_file(const String* filename);
// Read file chunk into string
String String_from_file_chunked(
    const String* filename,
    size_t chunk_size,
    size_t chunk_position
);
// Write entire String to file
void String_to_file(String* s, const String* filename);
// Write a chunk to a file
int String_to_file_chunked(
    const String* chunk,
    const String* filename,
    size_t chunk_size,
    size_t chunk_position
);

// These functions use strlen() an should only be used with c string constants
String String_from_cstr(const char* s);

// free responsiblity is given to the caller
char* String_to_cstr(const String* s);

typedef struct {
    const char* data;
    size_t len;
} StringView;

StringView
StringView_create(const String* s, size_t start_inc, size_t end_ninc);
StringView StringView_from_cstr(const char* data);

void String_push_sv(String* to, StringView from);

#endif
