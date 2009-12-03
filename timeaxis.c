/*
 * timeaxis.c
 */
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "gtool3.h"

#include "internal.h"

static int calendar = GT3_CAL_GREGORIAN;
static GT3_Date since = { 1950, 1, 1, 0, 0, 0};
static GT3_Duration interval = { 3, GT3_UNIT_HOUR };


struct idict {
    const char *key;
    int value;
};

static struct idict cdict[] = {
    { "gregorian", GT3_CAL_GREGORIAN },
    { "360_day", GT3_CAL_360_DAY },
    { "noleap", GT3_CAL_NOLEAP }
};


void
set_origin_year(int year)
{
    since.year = year;
}


int
set_timeinterval(const char *freq)
{
    struct { const char *key; int value; } dict[] = {
        { "hr",  GT3_UNIT_HOUR },
        { "da",  GT3_UNIT_DAY  },
        { "mon", GT3_UNIT_MON  },
        { "yr",  GT3_UNIT_YEAR },
    };
    char *endp;
    int i, value = 1, unit = -1;

    if (isdigit(freq[0])) {
        value = strtol(freq, &endp, 10);
        freq = endp;
    }

    for (i = 0; i < sizeof dict / sizeof dict[0]; i++) {
        if (strcmp(freq, dict[i].key) == 0) {
            unit = dict[i].value;
            break;
        }
    }
    if (unit >= 0) {
        interval.value = value;
        interval.unit = unit;
    }
    return unit;
}


int
set_calendar_by_name(const char *name)
{
    int i;

    for (i = 0; i < sizeof cdict / sizeof(struct idict); i++)
        if (strcmp(name, cdict[i].key) == 0) {
            calendar = cdict[i].value;
            return 0;
        }

    return -1;
}


double
get_time(const GT3_Date *date)
{
    return GT3_getTime(date, &since, GT3_UNIT_DAY, calendar);
}


int
get_calendar(void)
{
    return calendar;
}


void
step_time(GT3_Date *date)
{
    GT3_addDuration(date, &interval, calendar);
}


int
get_timeaxis(const cmor_axis_def_t *timedef)
{
    char tunit[128];
    int axis;

    snprintf(tunit, sizeof tunit - 1, "days since %d-1-1", since.year);

    if (cmor_axis(&axis, (char *)timedef->id, tunit, 1,
                  NULL, 'd', NULL, 0, NULL) != 0) {
        logging(LOG_ERR, "cmor_axis() failed.");
        return -1;
    }
    return axis;
}



#ifdef TEST_MAIN2
int
test_timeaxis(void)
{
    GT3_Date now;
    double time;
    int rval;

    set_calendar_by_name("gregorian");
    set_origin_year(0);

    GT3_setDate(&now, 1951, 1, 16, 0, 0, 0);
    time = get_time(&now);
    assert(time == 712603.);

    GT3_setDate(&now, 1951, 1, 16, 12, 0, 0);
    time = get_time(&now);
    assert(time == 712603.5);


    set_calendar_by_name("noleap");
    set_origin_year(0);

    GT3_setDate(&now, 115, 1, 16, 0, 0, 0);
    time = get_time(&now);
    assert(time == 41990.);

    GT3_setDate(&now, 115, 1, 16, 12, 0, 0);
    time = get_time(&now);
    assert(time == 41990.5);

    GT3_setDate(&now, 0, 1, 1, 1, 0, 0);
    time = get_time(&now);
    assert(time == 1. / 24);

    rval = set_timeinterval("mon");
    assert(rval >= 0);
    assert(interval.value == 1 && interval.unit == GT3_UNIT_MON);

    rval = set_timeinterval("3hr");
    assert(rval >= 0);
    assert(interval.value == 3 && interval.unit == GT3_UNIT_HOUR);

    rval = set_timeinterval("12hr");
    assert(rval >= 0);
    assert(interval.value == 12 && interval.unit == GT3_UNIT_HOUR);

    rval = set_timeinterval("yr");
    assert(rval >= 0);
    assert(interval.value == 1 && interval.unit == GT3_UNIT_YEAR);

    rval = set_timeinterval("");
    assert(rval < 0);

    printf("test_timeaxis(): DONE\n");
    return 0;
}
#endif /* TEST_MAIN2 */
