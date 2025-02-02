#include "String.h"
#include <stdlib.h>

int main()
{
    const char* success = "\x1b[32mpass\x1b[0m";
    const char* fail = "\x1b[31mfail\x1b[0m";

    {
        const char* test_name = "String_cmp true";
        String qnt = String_from_cstr("aaa");
        String ans = String_from_cstr("aaa");
        printf("%s: ", test_name);
        if (String_cmp(&qnt, &ans) == 0 && qnt.len == 3) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            String_dbprint(&qnt);
        }
        String_free(&qnt);
        String_free(&ans);
    }
    {
        const char* test_name = "String_cmp false at first index";
        String qnt = String_from_cstr("aaa");
        String ans = String_from_cstr("baa");
        printf("%s: ", test_name);
        if (String_cmp(&qnt, &ans) == 1 && qnt.len == 3) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            String_dbprint(&qnt);
        }
        String_free(&qnt);
        String_free(&ans);
    }
    {
        const char* test_name = "String_cmp false at second index";
        String qnt = String_from_cstr("aaa");
        String ans = String_from_cstr("aba");
        printf("%s: ", test_name);
        if (String_cmp(&qnt, &ans) == 2 && qnt.len == 3) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            String_dbprint(&qnt);
        }
        String_free(&qnt);
        String_free(&ans);
    }
    {
        const char* test_name = "String_cmp different lengths";
        String qnt = String_from_cstr("aaa");
        String ans = String_from_cstr("aaaa");
        printf("%s: ", test_name);
        if (String_cmp(&qnt, &ans) == -1 && qnt.len == 3) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            String_dbprint(&qnt);
        }
        String_free(&qnt);
        String_free(&ans);
    }
    {
        const char* test_name = "String_create basic";
        String qnt = String_create("aaa", 3);
        String ans = String_from_cstr("aaa");
        printf("%s: ", test_name);
        if (String_cmp(&qnt, &ans) == 0 && qnt.len == 3) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            String_dbprint(&qnt);
        }
        String_free(&qnt);
        String_free(&ans);
    }
    {
        const char* test_name = "String_create nothing";
        String qnt = String_create("", 0);
        String ans = String_from_cstr("");
        printf("%s: ", test_name);
        if (String_cmp(&qnt, &ans) == 0 && qnt.len == 0) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            String_dbprint(&qnt);
        }
        String_free(&qnt);
        String_free(&ans);
    }
    {
        const char* test_name = "String_push_move basic";
        String qnt = String_from_cstr("aaa");
        String_push_move(&qnt, String_from_cstr("bbb"));
        String ans = String_from_cstr("aaabbb");
        printf("%s: ", test_name);
        if (String_cmp(&qnt, &ans) == 0) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            String_dbprint(&qnt);
        }
        String_free(&qnt);
        String_free(&ans);
    }
    {
        const char* test_name = "String_push_move nothing";
        String qnt = String_from_cstr("aaa");
        String_push_move(&qnt, String_from_cstr(""));
        String ans = String_from_cstr("aaa");
        printf("%s: ", test_name);
        if (String_cmp(&qnt, &ans) == 0) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            String_dbprint(&qnt);
        }
        String_free(&qnt);
        String_free(&ans);
    }
    {
        const char* test_name = "String_push_copy basic";
        String qnt1 = String_from_cstr("aaa");
        String qnt2 = String_from_cstr("bbb");
        String_push_copy(&qnt1, &qnt2);
        String ans1 = String_from_cstr("aaabbb");
        String ans2 = String_from_cstr("bbb");
        printf("%s: ", test_name);
        if (String_cmp(&qnt1, &ans1) == 0 && String_cmp(&qnt2, &ans2) == 0) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            String_dbprint(&qnt1);
            String_dbprint(&qnt2);
        }
        String_free(&qnt1);
        String_free(&qnt2);
        String_free(&ans1);
        String_free(&ans2);
    }
    {
        const char* test_name = "String_to_file basic";
        String filename = String_from_cstr("outfile.txt");
        String contents_out = String_from_cstr("Hello file");
        String_to_file(&contents_out, &filename);
        String contents_in = String_from_file(&filename);
        printf("%s: ", test_name);
        if (String_cmp(&contents_out, &contents_in) == 0) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            String_dbprint_hex(&contents_out);
        }
        String_free(&contents_out);
        String_free(&contents_in);

        char* filename_cstr = String_to_cstr(&filename);
        int ret = remove(filename_cstr);
        if (ret < 0) {
            printf("failed to rm outfile.txt from String_test\n");
        }
        free(filename_cstr);
        String_free(&filename);
    }
    {
        const char* test_name = "String serialization basic";
        uint32_t to_ser = 0xF1F2F3F4;
        String qnt = String_new();
        String_push_u32(&qnt, to_ser);
        printf("%s: ", test_name);
        if (String_parse_u32(&qnt, 0) == to_ser) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            String_dbprint_hex(&qnt);
        }

        String_free(&qnt);
    }
    {
        const char* test_name = "String_to_file_chunked basic";
        String filename = String_from_cstr("outfile.txt");
        String contents_out = String_from_cstr("abcdefghij");
        String_to_file_chunked(&contents_out, &filename, contents_out.len, 10);
        String contents_in = String_from_file(&filename);
        String cmp_str = String_new();
        for (size_t i = 0; i < 10; i++) {
            String_push_back(&cmp_str, '\0');
        }
        String_push_move(&cmp_str, contents_out);

        printf("%s: ", test_name);
        if (String_cmp(&cmp_str, &contents_in) == 0) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            String_dbprint_hex(&contents_in);
            String_dbprint_hex(&cmp_str);
        }
        String_free(&contents_in);
        String_free(&cmp_str);

        char* filename_cstr = String_to_cstr(&filename);
        int ret = remove(filename_cstr);
        if (ret < 0) {
            printf("failed to rm outfile.txt from String_test\n");
        }
        free(filename_cstr);
        String_free(&filename);
    }
}
