/*
 * startswith.c
 */
#include <ctype.h>
#include "myutils.h"

/*
 * startswith() returns 1 if 's1'(the 1st argument) starts with 's2'
 * (the 2nd argument), otherwise 0.
 */
int
startswith(const char *s1, const char *s2)
{
    for (; *s2 != '\0'; s1++, s2++)
        if (*s1 != *s2)
            return 0;

    return 1;
}


/*
 * Same as startswith() except for case-insensitive.
 */
int
startswith_nocase(const char *s1, const char *s2)
{
    const unsigned char *us1 = (const unsigned char *)s1;
    const unsigned char *us2 = (const unsigned char *)s2;

    for (; *us2 != '\0'; us1++, us2++)
        if (toupper(*us1) != toupper(*us2))
            return 0;

    return 1;
}


#ifdef TEST_MAIN
#include <assert.h>

int
main(int argc, char **argv)
{
    assert(startswith_nocase("", ""));
    assert(startswith_nocase("/usr/local", ""));
    assert(startswith_nocase("/usr/local", "/"));
    assert(startswith_nocase("/usr/local", "/USR"));
    assert(startswith_nocase("/USR/local", "/usr"));
    assert(startswith_nocase("/usr/local", "/USR/LOCAL"));
    assert(startswith_nocase("/USR/LOCAL", "/usr/local"));

    assert(!startswith_nocase("", "/usr"));
    assert(!startswith_nocase("/usr/local", "/usr/local/bin"));
    assert(!startswith_nocase("/usr/local", "/bin"));
    return 0;
}
#endif /* TEST_MAIN */
