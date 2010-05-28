/*
 * unit.c
 */
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "myutils.h"

static char *var_unit = NULL;


void
unset_varunit(void)
{
    free(var_unit);
    var_unit = NULL;
}


int
set_varunit(const char *str)
{
    unset_varunit();
    var_unit = strdup(str);
    if (var_unit == NULL) {
        logging(LOG_SYSERR, NULL);
        return -1;
    }
    return 0;
}


int
rewrite_unit(char *unit, size_t size)
{
    if (var_unit) {
        logging(LOG_INFO, "rewrite unit [%s] => [%s].", unit, var_unit);
        strlcpy(unit, var_unit, size);
    }
    return 0;
}
