/*
 * strcase.c
 */
#include <ctype.h>


char *
toupper_string(char *str)
{
    char *p = str;

    while ((*p = toupper(*(unsigned char *)p)))
        p++;

    return str;
}


char *
tolower_string(char *str)
{
    char *p = str;

    while ((*p = tolower(*(unsigned char *)p)))
        p++;

    return str;
}
