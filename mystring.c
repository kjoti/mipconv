/*
 * mystring.c
 */
#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>




char *
trimmed_tail(const char *str)
{
    const char *p = str + strlen(str);

    while (p > str && isspace(*(p - 1)))
        --p;

    return (char *)p;
}


char *
strtrim(char *str)
{
    char *tail = trimmed_tail(str);
    *tail = '\0';

    return str;
}


/*
 * read_logicline() reads a logical line, which might be continued
 * if current line ends with '\\'.
 *
 * read_logicline() returns the number of characters copied into 'dest',
 * excluding null terminator.
 */
size_t
read_logicline(char *dest, size_t ndest, FILE *fp)
{
    size_t cnt = 0, len;
    char *ptr;
    char endchr;

    while (fgets(dest, ndest, fp)) {
        ptr = dest;
        while (isspace(*ptr))
            ptr++;
        if (ptr[0] == '\0')
            continue;

        strtrim(ptr);
        len = strlen(ptr);
        endchr = ptr[len - 1];

        /* remove '\\' at end of line. */
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


#if 1
int
main(int argc, char **argv)
{
    char aline[4096];
    size_t len;

    while ((len = read_logicline(aline, sizeof aline, stdin)) > 0) {
        printf("%6u (%s)\n", len, aline);
    }
    return 0;
}
#endif