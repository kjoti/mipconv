/*
 *  main.c -- data converter for CMIP5 (from gtool3 to netcdf).
 *
 *  2009
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include "logging.h"
#include "gtool3.h"
#include "cmor.h"
#include "cmor_supp.h"

#include "internal.h"

#define PROGNAME "mipconv"


static int
process_args(int argc, char **argv)
{
    int rval = 0;
    char *vname = NULL;
    int cnt = 0;


    for (; argc > 0 && *argv; argc--, argv++) {
        if (*argv[0] == ':') {
            vname = *argv + 1;
            cnt = 0;
            continue;
        }

        logging(LOG_INFO, "converting %s (%s)", *argv, vname);
        if (convert(vname, *argv, cnt) < 0) {
            rval = -1;
            break;
        }
        cnt++;
    }
    return rval;
}


void
usage(void)
{
    fprintf(stderr,
            "usage: %s [option] mip-table [:var-name gtool3-files...]\n",
            PROGNAME);
}


int
main(int argc, char **argv)
{
    int ch, rval = 0;
    int status, table_id;

    open_logging(stderr, PROGNAME);
    GT3_setProgname(PROGNAME);

    while ((ch = getopt(argc, argv, "hv")) != -1)
        switch (ch) {
        case 'v':
            set_logging_level("verbose");
            break;
        case 'h':
        default:
            usage();
            exit(1);
        }

    argc -= optind;
    argv += optind;
    if (argc < 1) {
        logging(LOG_ERR, "Required argument(MIP-table) is missing.");
        usage();
        exit(1);
    }

    setup();
    status = cmor_load_table(*argv, &table_id);
    if (status != 0) {
        logging(LOG_ERR, "cmor_load_table() failed");
        exit(1);
    }
    logging(LOG_INFO, "MIP-TALBE: %s", *argv);

    argv++;
    argc--;
    rval = process_args(argc, argv);
    cmor_close();

    return rval < 0 ? 1 : 0;
}
