/*
 * startswith.c
 */

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
