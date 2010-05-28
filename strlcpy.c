/*
 * strlcpy.c
 */
#include <stdlib.h>

#ifndef HAVE_STRLCPY
#include "myutils.h"

/* Note: First appeared in OpenBSD 2.4 */
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

size_t
strlcat(char *dest, const char *src, size_t size)
{
    size_t cnt = 0;

    while (cnt < size && *dest != '\0') {
        cnt++;
        dest++;
    }

    return cnt + strlcpy(dest, src, size - cnt);
}
#endif
