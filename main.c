/*
 * main.c -- data converter for CMIP5 (from gtool3 to netcdf).
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "netcdf.h"

#include "logging.h"
#include "gtool3.h"
#include "cmor.h"
#include "cmor_supp.h"

#include "internal.h"

#define PROGNAME "mipconv"


static int
process_args(int argc, char **argv)
{
    const char optswitch[] = "epu";
    int rval = 0;
    char *vname = NULL;
    int cnt = 0;


    for (; argc > 0 && *argv; argc--, argv++) {
        if (*argv[0] == ':') {
            unset_varunit();
            unset_calcexpr();
            unset_positive();
            vname = *argv + 1;
            cnt++;
            logging(LOG_INFO, "variable name: (%s)", vname);
            continue;
        }

        if (*argv[0] == '=' && strchr(optswitch, argv[0][1])) {
            switch (argv[0][1]) {
            case 'e':
                if (set_calcexpr(*argv + 2) < 0)
                    return -1;

                logging(LOG_INFO, "Expr specified: [%s]", *argv + 2);
                break;
            case 'p':
                if (set_positive(*argv + 2) < 0)
                    return -1;
                break;
            case 'u':
                if (set_varunit(*argv + 2) < 0)
                    return -1;

                logging(LOG_INFO, "Unit specified: [%s]", *argv + 2);
                break;
            default:
                assert(!"NOTREACHED");
                break;
            }
            continue;
        }

        if (cnt == 0) {
            logging(LOG_ERR, "No variable name specified.");
            rval = -1;
            break;
        }

        logging(LOG_INFO, "input file: (%s)", *argv);
        if (convert(vname, *argv, cnt) < 0) {
            logging(LOG_ERR, "%s: failed", *argv);
            rval = -1;
            break;
        }
        vname = NULL;
    }
    return rval;
}


static void
usage(void)
{
    const char *usage_message =
        "Usage: " PROGNAME " [options] MIP-Table :var-name files...\n"
        "\n"
        "Options:\n"
        "    -d DIR       specify output directory\n"
        "    -f conffile  specify configuration file\n"
        "    -v           verbose mode\n"
        "    -z LIST      specify z-level slice\n"
        "\n";

    fprintf(stderr,
            "Versions:\n"
            "    main: %s\n"
            "    libgtool3: %s\n"
            "    netcdf library: %s\n"
            "\n",
            mipconv_version(), GT3_version(), nc_inq_libvers());
    fprintf(stderr, usage_message);
}


int
main(int argc, char **argv)
{
    int ch, rval = 0;
    int status, table_id;
    FILE *fp = NULL;


    open_logging(stderr, PROGNAME);
    GT3_setProgname(PROGNAME);

    while ((ch = getopt(argc, argv, "d:f:vz:h")) != -1)
        switch (ch) {
        case 'd':
            if (set_outdir(optarg) < 0)
                exit(1);
            break;
        case 'f':
            if ((fp = fopen(optarg, "r")) == NULL) {
                logging(LOG_SYSERR, optarg);
                exit(1);
            }
            if (read_config(fp) < 0) {
                logging(LOG_ERR, "in configuration file (%s)", optarg);
                exit(1);
            }
            fclose(fp);
            break;
        case 'v':
            set_logging_level("verbose");
            break;
        case 'z':
            if (set_axis_slice(2, optarg) < 0) {
                usage();
                exit(1);
            }
            break;
        case 'h':
            usage();
            exit(0);
            break;
        default:
            usage();
            exit(1);
        }

    argc -= optind;
    argv += optind;
    if (argc < 1) {
        usage();
        exit(1);
    }

    if (setup() < 0)
        exit(1);

    logging(LOG_INFO, "loading MIP-table (%s)", *argv);
    status = cmor_load_table(*argv, &table_id);
    if (status != 0) {
        logging(LOG_ERR, "cmor_load_table() failed");
        exit(1);
    }

    argv++;
    argc--;
    rval = process_args(argc, argv);
    cmor_close();
    logging(LOG_INFO, "CMOR closed");
    if (rval == 0)
        logging(LOG_INFO, "SUCCESSFUL END");

    return rval < 0 ? 1 : 0;
}
