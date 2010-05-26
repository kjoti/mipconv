/*
 * split2.c
 */
#include <string.h>

#include "myutils.h"

/*
 * split2() splits a string into two parts which are
 * delimited by characters (delim).
 *
 * Typical use is splitting into a key/value pair.
 */
char *
split2(char *key, size_t keylen, const char *ptr, const char *delim)
{
    while (*ptr != '\0' && strchr(delim, *ptr) == NULL) {
        if (keylen > 1) {
            *key++ = *ptr;
            keylen--;
        }
        ptr++;
    }
    if (keylen > 0)
        *key = '\0';

    while (*ptr != '\0' && strchr(delim, *ptr))
        ptr++;

    return (char *)ptr;
}


#ifdef TEST_MAIN
#include <assert.h>

static void
test1(const char *data, const char *s1, const char *s2, const char *delim)
{
    char key[32];
    char *value;

    value = split2(key, sizeof key, data, delim);

    assert(strcmp(key, s1) == 0);
    assert(strcmp(value, s2) == 0);

    value = split2(NULL, 0, data, delim);
    assert(strcmp(value, s2) == 0);
}

static void
test2(const char *data, const char *s1, const char *s2, const char *delim)
{
    char key[3];
    char *value;

    value = split2(key, sizeof key, data, delim);

    assert(strcmp(key, s1) == 0);
    assert(strcmp(value, s2) == 0);

    value = split2(NULL, 0, data, delim);
    assert(strcmp(value, s2) == 0);
}


int
main(int argc, char **argv)
{
    test1("name   foo bar", "name", "foo bar", " ");

    test1("name:   foo bar", "name:", "foo bar", " ");
    test1("name:   foo bar", "name", "foo bar", " :");
    test1("name:   foo bar", "name", "foo bar", ": ");

    test1("name:foo bar", "name", "foo bar", ": ");
    test1("name foo bar", "name", "foo bar", ": ");

    test1("name foo bar", "name foo bar", "", ":");

    test1("  ", "", "", ": ");
    test1("", "", "", ": ");

    test2("name  foo bar", "na", "foo bar", " ");

    return 0;
}
#endif
