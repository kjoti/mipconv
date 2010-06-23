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
    const char optswitch[] = "cepuz";
    int rval = 0;
    char *vname = NULL;
    int cnt = 0;


    for (; argc > 0 && *argv; argc--, argv++) {
        if (*argv[0] == ':') {
            unset_varunit();
            unset_calcexpr();
            unset_positive();
            sdb_close();
            unset_axis_slice();
            vname = *argv + 1;
            cnt++;
            logging(LOG_INFO, "variable name: (%s)", vname);
            continue;
        }

        if (*argv[0] == '=' && strchr(optswitch, argv[0][1])) {
            switch (argv[0][1]) {
            case 'c':
                if (sdb_open(*argv + 2) < 0)
                    return -1;
                break;
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
            case 'z':
                if (set_axis_slice(2, *argv + 2) < 0)
                    return -1;
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
        "Usage: " PROGNAME
        " [options] MIP-Table :var-name [voption] files...\n"
        "\n"
        "Options:\n"
        "    -3           use netCDF3 format.\n"
        "    -d DIR       specify output directory.\n"
        "    -f conffile  specify global attribute file.\n"
        "    -m mode      specify writing mode(\"preserve\" or \"replace\").\n"
        "                 (default: \"replace\")\n"
        "    -v           verbose mode.\n"
        "    -h           print this message.\n"
        "\n";
    const char *usage_message2 =
        "Var Options:\n"
        "    =c...        specify comment file.\n"
        "    =e...        specify expression.\n"
        "    =p...        specify 'up' or 'down'.\n"
        "    =u...        specify unit.\n"
        "    =z...        specify z-level slice.\n"
        "\n";

    fprintf(stderr,
            "Versions:\n"
            "    main: %s\n"
            "    libgtool3: %s\n"
            "    netcdf library: %s\n"
            "\n",
            mipconv_version(), GT3_version(), nc_inq_libvers());
    fprintf(stderr, usage_message);
    fprintf(stderr, usage_message2);
}


int
main(int argc, char **argv)
{
    int ch, rval = 0;
    int status, table_id;
    FILE *fp = NULL;


    open_logging(stderr, PROGNAME);
    GT3_setProgname(PROGNAME);

    while ((ch = getopt(argc, argv, "34d:f:m:vh")) != -1)
        switch (ch) {
        case '3':
            use_netcdf(3);
            break;
        case '4':
            use_netcdf(4);
            break;
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
        case 'm':
            if (set_writing_mode(optarg) < 0) {
                logging(LOG_ERR, "%s: unknown mode", optarg);
                exit(1);
            }
            break;

        case 'v':
            set_logging_level("verbose");
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
    logging(LOG_INFO, rval == 0 ? "SUCCESSFUL END" : "ABNORMAL END");
    return rval < 0 ? 1 : 0;
}
