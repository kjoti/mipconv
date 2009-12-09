/*
 * mystring.c
 */
#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>


size_t
strlcpy(char *dest, const char *src, size_t size)
{
    const char *s = src;

    while (size > 1 && (*dest++ = *s++))
        --size;

    if (size <= 1) {
        if (size == 1)
            *dest = '\0';

        while (*s++)
            ;
    }
    return (s - src - 1);
}


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


char *
endswith(const char *str, const char *sub)
{
    int head = strlen(str) - strlen(sub);

    return (head >= 0 && strcmp(str + head, sub) == 0)
        ? (char *)str + head
        : NULL;
}


/*
 * read_logicline() reads a logical line, which might be continued
 * if current line ends with '\\'.
 */
size_t
read_logicline(char *dest, size_t ndest, FILE *fp)
{
    size_t cnt = 0, len;
    char *ptr;

    while (fgets(dest, ndest, fp)) {
        ptr = dest;
        while (isspace(*ptr))
            ptr++;
        if (ptr[0] == '\0')
            continue;

        strtrim(ptr);
        len = strlen(ptr);

        /*
         * join a next line if current line ends with '\\'.
         */
        if (ptr[len - 1] == '\\') {
            len--;
            if (dest != ptr)
                memmove(dest, ptr, len);

            dest += len;
            ndest -= len;
            cnt += len;
            continue;
        }

        if (dest != ptr)
            memmove(dest, ptr, len);

        dest += len;
        ndest -= len;
        cnt += len;
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
