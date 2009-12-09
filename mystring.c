/*
 * mystring.c
 */
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
    char *buf, buf2[128];
    size_t cnt = 0, len;
    char *ptr;

    while (fgets(buf2, sizeof buf2, fp)) {
        buf = buf2;
        while (isspace(*buf))
            buf++;
        if (buf[0] == '\0')
            continue;

        strtrim(buf);

        /*
         * join a next line if current line ends with '\\'.
         */
        if ((ptr = endswith(buf, "\\")) != NULL) {
            *ptr = '\0';        /* overwrite '\\' */
            len = strlcpy(dest, buf, ndest);
            cnt += len;

            if (len >= ndest)
                len = ndest;

            dest += len;
            ndest -= len;
            continue;
        }

        len = strlcpy(dest, buf, ndest);
        cnt += len;
        break;
    }
    return cnt;
}

#if 1
int
main(int argc, char **argv)
{
    char aline[1024];
    size_t len;

    while ((len = read_logicline(aline, sizeof aline, stdin)) > 0) {
        printf("%6u (%s)\n", len, aline);
    }
    return 0;
}
#endif
