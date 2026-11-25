/*
 * timeaxis.c
 *
 * keep calendar & basetime.
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
static GT3_Date basetime = { 1, 1, 1, 0, 0, 0};

struct idict {
    const char *key;
    int value;
};

/*
 * Mapping from names in CF-Conventions to GT3_CAL_* (libgtool3).
 */
static struct idict cdict[] = {
    { "proleptic_gregorian", GT3_CAL_GREGORIAN },
    { "gregorian", GT3_CAL_GREGORIAN },
    { "360_day", GT3_CAL_360_DAY },
    { "noleap", GT3_CAL_NOLEAP },
    { "standard", GT3_CAL_GREGORIAN },
    { "julian", GT3_CAL_JULIAN },
};


int
set_basetime(const char *str)
{
    GT3_Date temp;

    if (set_date_by_string(&temp, str) < 0
        || temp.hour < 0 || temp.hour >= 24
        || temp.min  < 0 || temp.min  >= 60
        || temp.sec  < 0 || temp.sec  >= 60)
        return -1;

    if (GT3_checkDate(&temp, calendar) < 0)
        return -1;

    basetime = temp;
    return 0;
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
    return GT3_getTime(date, &basetime, GT3_UNIT_DAY, calendar);
}


int
get_calendar(void)
{
    return calendar;
}


void
step_time(GT3_Date *date, const GT3_Duration *tdur)
{
    GT3_addDuration(date, tdur, calendar);
}


int
get_timeaxis(const cmor_axis_def_t *timedef)
{
    const int UNLIMITED = 0;
    char tunit[128];
    int axis;

    if (basetime.hour == 0 && basetime.min == 0 && basetime.sec == 0)
        snprintf(tunit, sizeof tunit - 1,
                 "days since %d-%d-%d",
                 basetime.year, basetime.mon, basetime.day);
    else
        snprintf(tunit, sizeof tunit - 1,
                 "days since %d-%d-%d %02d:%02d:%02d",
                 basetime.year, basetime.mon, basetime.day,
                 basetime.hour, basetime.min, basetime.sec);

    if (cmor_axis(&axis, (char *)timedef->id, tunit, UNLIMITED,
                  NULL, 'd', NULL, 0, NULL) != 0) {
        logging(LOG_ERR, "cmor_axis() failed.");
        return -1;
    }
    logging(LOG_INFO, "units of time: %s", tunit);
    return axis;
}


int
check_duration(const GT3_Duration *tdur,
               const GT3_Date *date1,
               const GT3_Date *date2)
{
    GT3_Date date;

    GT3_copyDate(&date, date1);
    GT3_addDuration(&date, tdur, calendar);

    return GT3_cmpDate2(date2, &date) == 0 ? 0 : -1;
}


#ifdef TEST_MAIN2
int
test_timeaxis(void)
{
    GT3_Date now;
    double time;

    set_basetime("2000-1-1");

    set_calendar_by_name("proleptic_gregorian");
    GT3_setDate(&now, 1999, 12, 31, 12, 0, 0);
    time = get_time(&now);
    assert(time == -0.5);

    GT3_setDate(&now, 2400, 1, 1, 0, 0, 0);
    time = get_time(&now);
    assert(time == 400 * 365. + 97.);

    GT3_setDate(&now, 2001, 1, 2, 0, 0, 0);
    time = get_time(&now);
    assert(time == 367.);

    set_calendar_by_name("noleap");
    time = get_time(&now);
    assert(time == 366.);

    set_calendar_by_name("360_day");
    time = get_time(&now);
    assert(time == 361.);

    printf("test_timeaxis(): DONE\n");
    return 0;
}
#endif /* TEST_MAIN2 */
