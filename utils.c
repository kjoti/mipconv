#include <stdio.h>
#include <assert.h>
#include "internal.h"

int
snprint_gt3date(char *buf, size_t size, const GT3_Date *date)
{
    return snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d",
                    date->year, date->mon, date->day,
                    date->hour, date->min, date->sec);
}
