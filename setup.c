/*
 *  setup.c -- setup of data converter for CMIP5.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "logging.h"
#include "cmor.h"

#include "internal.h"


void
setup(void)
{
    int status;
    int netcdf = CMOR_REPLACE_3;

    status = cmor_setup(NULL, &netcdf, NULL, NULL, "cmor.log", NULL);
    if (status != 0) {
        logging(LOG_ERR, "cmor_setup() failed");
        exit(1);
    }

    status = cmor_dataset(
        "/tmp/kjo",
        "pre-industrial control",
        "CCSR+NIES+FRCGC",
        "MIROC4.0",
        "360_day",
        1,
        "emori@nies.go.jp",
        "2009",
        "hello, world",
        "not yet",
        0,
        0,
        NULL,
        "MIROC",
        "RCP",
        1, 1,
        "CCSR+NIES+FRCGC");

    if (status != 0) {
        logging(LOG_ERR, "cmor_dataset(): failed");
        exit(1);
    }
}
