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
    fprintf(stderr, "usage: test user_input.json table.json\n");
}


int
main(int argc, char **argv)
{
    open_logging(stderr, PROGNAME);
    GT3_setProgname(PROGNAME);
    set_logging_level("verbose");

    argv++;
    argc--;
    if (argc != 2) {
        usage();
        exit(1);
    }
    if (setup(NULL, NULL, *argv) < 0) {
        exit(1);
    }
    argv++;
    argc--;
    if (load_normal_table(*argv) < 0) {
        logging(LOG_ERR, "cmor_load_table() failed.");
        exit(1);
    }
    logging(LOG_INFO, "MIP-TALBE: %s", *argv);

    printf("cmor_ntables: %d\n", cmor_ntables);
    {
        cmor_table_t *table = &cmor_tables[CMOR_TABLE];

        printf("  table->nvars: %d\n", table->nvars);
        printf("  table->naxes: %d\n", table->naxes);
        printf("  table->nformula: %d\n", table->nformula);
    }
    test_cmor_supp();
    test_axis();
    test_timeaxis();
    test_converter();
    test_zfactor();
    test_calculator();
    test_zfactor();
    test_coord();
    /* test_rotated_pole(); */
    test_bipolar();

    printf("ALL TESTS DONE\n");
    return 0;
}
