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

static int calendar = GT3_CAL_DUMMY;
static GT3_Date basetime = { 1, 1, 1, 0, 0, 0};

struct idict {
    const char *key;
    int value;
};

static struct idict cdict[] = {
    { "gregorian", GT3_CAL_GREGORIAN },
    { "360_day", GT3_CAL_360_DAY },
    { "noleap", GT3_CAL_NOLEAP },
    { "standard", GT3_CAL_GREGORIAN },
    { "proleptic_gregorian", GT3_CAL_GREGORIAN },
    { "julian", GT3_CAL_JULIAN }
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

    /*
     * XXX: We cannot validate the date here because calendar is
     * unknown yet. Validation is done by chekc_basetime() in later.
     */
    basetime = temp;
    return 0;
}


int
check_basetime(void)
{
    return GT3_checkDate(&basetime, calendar);
}


void
set_origin_year(int year)
{
    basetime.year = year;
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


/*
 * set calendar after cmor_setup().
 */
int
set_calendar(int cal)
{
    const char *names[] = {
        "gregorian",
        "noleap",
        "all_leap",
        "360_day",
        "julian"
    };
    const char *p;

    if (cal == GT3_CAL_GREGORIAN && basetime.year < 1583)
        p = "proleptic_gregorian";
    else {
        assert(cal >= 0 && cal < 5);
        p = names[cal];
    }

    /*
     * XXX: Since CMOR 2.5.3, this call fails.
     */
    if (cmor_set_cur_dataset_attribute("calendar", (char *)p ,0) != 0) {
        logging(LOG_ERR, "cmor_set_cur_dataset_attribute() failed.");
        return -1;
    }
    logging(LOG_INFO, "set calendar (%s).", p);
    calendar = cal;
    return 0;
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


    printf("test_timeaxis(): DONE\n");
    return 0;
}
#endif /* TEST_MAIN2 */
