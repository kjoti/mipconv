/*
 * strcasecmp.c
 */
#include <ctype.h>


#ifndef HAVE_STRCASECMP
int
strcasecmp(const char *s1, const char *s2)
{
    /*
     * If an argument of `toupper` is not an unsigned char value,
     * its behavior is undefiend.
     */
    const unsigned char *us1 = (const unsigned char *)s1;
    const unsigned char *us2 = (const unsigned char *)s2;
    int ch1, ch2;

    for (;;) {
        ch1 = toupper(*us1++);
        ch2 = toupper(*us2++);
        if (ch1 != ch2 || ch1 == 0)
            return ch1 - ch2;
    }
    /* NOTREACHED */
    return -1;
}
#endif /* !HAVE_STRCASECMP */


#ifdef TEST_MAIN
#include <assert.h>

int
main(int argc, char **argv)
{
    assert(strcasecmp("", "") == 0);

    assert(strcasecmp("!#0123abc{}", "!#0123abc{}") == 0);
    assert(strcasecmp("!#0123abc{}", "!#0123ABC{}") == 0);
    assert(strcasecmp("!#0123ABC{}", "!#0123abc{}") == 0);

    assert(strcasecmp("!#0123abc{}", "!#0123xyz{}") < 0);
    assert(strcasecmp("!#0123ABC{}", "!#0123xyz{}") < 0);
    assert(strcasecmp("!#0123abc{}", "!#0123XYZ{}") < 0);

    assert(strcasecmp("0123456789", "") > 0);
    assert(strcasecmp("", "0123456789") < 0);

    return 0;
}
#endif /* TEST_MAIN */
