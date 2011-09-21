/*
 * date.c
 */
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "gtool3.h"


static char *
getint(int *value, const char *ptr)
{
    int temp;
    char *endptr;

    temp = (int)strtol(ptr, &endptr, 10);
    if (ptr == endptr)
        return NULL;

    *value = temp;
    return endptr;
}


/*
 * parse date string: YYYY[-MM[-DD]] [hh[:mm[:ss]]]
 */
int
set_date_by_string(GT3_Date *date, const char *input)
{
    char *ptr = (char *)input;
    char *endptr;
    int year, mon = 1, day = 1, hour = 0, min = 0, sec = 0;

    /* YYYY-MM-DD */
    endptr = getint(&year, ptr);
    if (endptr == NULL)
        return -1; /* 'year' must be specified. */

    if (*endptr == '-') {
        ptr = endptr + 1;
        endptr = getint(&mon, ptr);
        if (endptr && *endptr == '-') {
            ptr = endptr + 1;
            endptr = getint(&day, ptr);
        }
    }

    /* hh:mm:ss */
    if (endptr && isspace(*endptr)) {
        ptr = endptr + 1;
        endptr = getint(&hour, ptr);
        if (endptr && *endptr == ':') {
            ptr = endptr + 1;
            endptr = getint(&min, ptr);
            if (endptr && *endptr == ':') {
                ptr = endptr + 1;
                endptr = getint(&sec, ptr);
            }
        }
    }

    GT3_setDate(date, year, mon, day, hour, min, sec);
    return 0;
}


#ifdef TEST_MAIN
#include <assert.h>

void
test(const char *str, int y, int m, int d, int hh, int mm, int ss)
{
    GT3_Date date;

    set_date_by_string(&date, str);

    assert(date.year == y);
    assert(date.mon == m);
    assert(date.day == d);
    assert(date.hour == hh);
    assert(date.min == mm);
    assert(date.sec == ss);
}


int
main(int argc, char **argv)
{
    test(" 2011 ", 2011, 1, 1, 0, 0, 0);
    test("2011-3-9", 2011, 3, 9, 0, 0, 0);
    test(" 2011-03-09 ", 2011, 3, 9, 0, 0, 0);

    test("2011-03-09     23", 2011, 3, 9, 23, 0, 0);
    test(" 2011-03-09    23:59", 2011, 3, 9, 23, 59, 0);
    test("  2011-03-09   23:59:50", 2011, 3, 9, 23, 59, 50);
    return 0;
}
#endif /* TEST_MAIN */
