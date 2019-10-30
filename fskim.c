/*
 * fskim.c
 */
#include <stdio.h>
#include "myutils.h"


/*
 * fskim()
 *
 * Read characters from a stream until finding `endchar`
 * and store them (including `endchar`) into the buffer.
 * If given buffer ran out, discard characters which cannot be stored.
 *
 * In successful end, return the number of characters which are read.
 * If an error has occurred, retrn -1.
 */
ssize_t
fskim(char *buf, size_t bufsize, int endchar, FILE *fp)
{
    size_t nread = 0;
    int ch;

    for (;;) {
        ch = fgetc(fp);
        if (ch == EOF) {
            if (ferror(fp)) {
                *buf = '\0';
                return -1;
            }
            break;
        }
        nread++;

        /* If no spaces, discard read character. */
        if (bufsize > 1) {
            *buf++ = (char)ch;
            bufsize--;
        }
        if (ch == endchar)
            break;
    }
    if (bufsize > 0)
        *buf = '\0';

    return nread;
}
