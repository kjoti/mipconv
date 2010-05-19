#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"

#include "cmor.h"
#include "cmor_supp.h"

#include "internal.h"

#define PROGNAME "mipconv-test"


void
usage(void)
{
    fprintf(stderr, "usage: test MIPTABLE\n");
}


int
main(int argc, char **argv)
{
    int status, table_id;

    open_logging(stderr, PROGNAME);
    GT3_setProgname(PROGNAME);
    set_logging_level("verbose");

    argv++;
    argc--;
    if (argc != 1) {
        usage();
        exit(1);
    }
    if (setup() < 0) {
        exit(1);
    }

    status = cmor_load_table(*argv, &table_id);
    if (status != 0) {
        logging(LOG_ERR, "cmor_load_table() failed");
        exit(1);
    }
    logging(LOG_INFO, "MIP-TALBE: %s", *argv);

    test_cmor_supp();
    test_axis();
    test_timeaxis();
    test_converter();
    test_zfactor();
    test_calculator();
    test_zfactor();
    printf("ALL TESTS DONE\n");
    return 0;
}
