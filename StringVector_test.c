#include "String.h"
#include "StringVector.h"

int main()
{
    const char* success = "\x1b[32mpass\x1b[0m";
    const char* fail = "\x1b[31mfail\x1b[0m";

    {
        const char* test_name = "StringVector_push_back_move basic";
        StringVector qnt = StringVector_new();
        StringVector_push_back_move(&qnt, String_from_cstr("first entry"));
        String ans = String_from_cstr("first entry");

        printf("%s: ", test_name);
        if (String_cmp(StringVector_get(&qnt, 0), &ans) == 0 && qnt.len == 1) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            StringVector_dbprint(&qnt);
        }
        StringVector_free(&qnt);
        String_free(&ans);
    }
    {
        const char* test_name = "StringVector_push_back_copy basic";
        StringVector qnt = StringVector_new();
        String first_list = String_from_cstr("first entry");
        StringVector_push_back_copy(&qnt, &first_list);

        printf("%s: ", test_name);
        if (String_cmp(StringVector_get(&qnt, 0), &first_list) == 0 &&
            qnt.len == 1) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            StringVector_dbprint(&qnt);
        }
        StringVector_free(&qnt);
        String_free(&first_list);
    }
    {
        const char* test_name = "StringVector_from_split basic";
        String to_split = String_from_cstr("abc\nabs\nfds");
        StringVector qnt = StringVector_from_split(&to_split, '\n');

        String f0 = String_from_cstr("abc");
        String f1 = String_from_cstr("abs");
        String f2 = String_from_cstr("fds");

        printf("%s: ", test_name);
        if (String_cmp(StringVector_get(&qnt, 0), &f0) == 0 &&
            String_cmp(StringVector_get(&qnt, 1), &f1) == 0 &&
            String_cmp(StringVector_get(&qnt, 2), &f2) == 0) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            StringVector_dbprint(&qnt);
        }
        StringVector_free(&qnt);
        String_free(&to_split);
        String_free(&f0);
        String_free(&f1);
        String_free(&f2);
    }
    {
        const char* test_name = "StringVector_from_split split char at end";
        String to_split = String_from_cstr("abc\nabs\nfds\n");
        StringVector qnt = StringVector_from_split(&to_split, '\n');

        String f0 = String_from_cstr("abc");
        String f1 = String_from_cstr("abs");
        String f2 = String_from_cstr("fds");

        printf("%s: ", test_name);
        if (String_cmp(StringVector_get(&qnt, 0), &f0) == 0 &&
            String_cmp(StringVector_get(&qnt, 1), &f1) == 0 &&
            String_cmp(StringVector_get(&qnt, 2), &f2) == 0) {
            printf("%s\n", success);
        } else {
            printf("%s\n", fail);
            StringVector_dbprint(&qnt);
        }
        StringVector_free(&qnt);
        String_free(&to_split);
        String_free(&f0);
        String_free(&f1);
        String_free(&f2);
    }
}
