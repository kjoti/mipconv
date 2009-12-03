#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"

#include "cmor.h"
#include "cmor_supp.h"

#include "internal.h"


int
main(int argc, char **argv)
{
    int status, table_id;

    argv++;
    argc--;
    assert(argc == 1);

    setup();
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

    printf("ALL TESTS DONE\n");
    return 0;
}
