#include "mstring.h"

int main()
{
    mstring_t qtn;
    mstring_t ans;

    // insertn after end
    mstring_init(&qtn);
    mstring_append(&qtn, "aaa");
    mstring_insertn(&qtn, "bbb", 4, 3);

    mstring_init(&ans);
    mstring_append(&ans, "aaabbb");

    printf(
        "insert after end: %s\n",
        (mstring_cmp(&qtn, &ans) == 0) ? "pass" : "fail"
    );

    mstring_free(&qtn);
    mstring_free(&ans);

    // insertn at 0
    mstring_init(&qtn);
    mstring_append(&qtn, "aaa");
    mstring_insertn(&qtn, "bbb", 0, 3);

    mstring_init(&ans);
    mstring_append(&ans, "bbbaaa");

    printf(
        "insert at 0: %s\n",
        (mstring_cmp(&qtn, &ans) == 0) ? "pass" : "fail"
    );

    mstring_free(&qtn);
    mstring_free(&ans);

    // insertn at 1
    mstring_init(&qtn);
    mstring_append(&qtn, "aaa");
    mstring_insertn(&qtn, "bbb", 1, 3);

    mstring_init(&ans);
    mstring_append(&ans, "abbbaa");

    printf(
        "insert at 1: %s\n",
        (mstring_cmp(&qtn, &ans) == 0) ? "pass" : "fail"
    );

    mstring_free(&qtn);
    mstring_free(&ans);

    // insertn at 1
    mstring_init(&qtn);
    mstring_append(&qtn, "aaa");
    mstring_insertn(&qtn, "bbb", 2, 3);

    mstring_init(&ans);
    mstring_append(&ans, "aabbba");

    printf(
        "insert at 2: %s\n",
        (mstring_cmp(&qtn, &ans) == 0) ? "pass" : "fail"
    );

    mstring_free(&qtn);
    mstring_free(&ans);

    // insertn at end
    mstring_init(&qtn);
    mstring_append(&qtn, "aaa");
    mstring_insertn(&qtn, "bbb", 3, 3);

    mstring_init(&ans);
    mstring_append(&ans, "aaabbb");

    printf(
        "insert at end: %s\n",
        (mstring_cmp(&qtn, &ans) == 0) ? "pass" : "fail"
    );

    mstring_free(&qtn);
    mstring_free(&ans);

    // insertn large at 0
    mstring_init(&qtn);
    mstring_append(&qtn, "aaa");
    mstring_insertn(&qtn, "bbbbbbbbbbbbbbbbbbbb", 0, 20);

    mstring_init(&ans);
    mstring_append(&ans, "aaabbbbbbbbbbbbbbbbbbbb");

    printf(
        "insert large at 0: %s\n",
        (mstring_cmp(&qtn, &ans) == 0) ? "pass" : "fail"
    );

    mstring_free(&qtn);
    mstring_free(&ans);

    // insertn large at 2
    mstring_init(&qtn);
    mstring_append(&qtn, "aaa");
    mstring_insertn(&qtn, "bbbbbbbbbbbbbbbbbbbb", 2, 20);

    mstring_init(&ans);
    mstring_append(&ans, "aabbbbbbbbbbbbbbbbbbbba");

    printf(
        "insert large at 2: %s\n",
        (mstring_cmp(&qtn, &ans) == 0) ? "pass" : "fail"
    );

    mstring_free(&qtn);
    mstring_free(&ans);
}
