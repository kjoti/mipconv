/*
 * logicline.c
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>


char *
trimmed_tail(const char *str)
{
    const char *p = str + strlen(str);

    while (p > str && isspace(*(p - 1)))
        --p;

    return (char *)p;
}


/*
 * read_logicline() reads a logical line, which might be continued
 * by a backslash at EOL.
 *
 * read_logicline() returns the number of characters copied into 'dest',
 * excluding null terminator. Note that returning zero does not mean EOF.
 */
size_t
read_logicline(char *dest, size_t ndest, FILE *fp)
{
    size_t cnt = 0, len;
    char *ptr;
    char endchr;

    ndest--;
    while (ndest > 0 && fgets(dest, ndest, fp)) {
        ptr = dest;
        while (isspace(*ptr))
            ptr++;
        if (ptr[0] == '\0')
            break;

        len = trimmed_tail(ptr) - ptr;
        endchr = ptr[len - 1];

        /* remove '\\' at the end of line. */
        if (endchr == '\\')
            len--;

        if (dest != ptr)
            memmove(dest, ptr, len);

        dest += len;
        ndest -= len;
        cnt += len;
        if (endchr != '\\')
            break;
    }
    *dest = '\0';
    return cnt;
}


#ifdef TEST_MAIN
int
main(int argc, char **argv)
{
    char aline[4096];
    size_t len;

    while (!feof(stdin)) {
        len = read_logicline(aline, sizeof aline, stdin);
        printf("%6u (%s)\n", len, aline);
    }
    return 0;
}
#endif
