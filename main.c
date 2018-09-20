/*
 * main.c -- data converter using CMOR3 (from gtool3 to netcdf).
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
    const char optswitch[] = "ceptuzH";
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
            unset_header_edit();
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
            case 't':
                if (set_time_slice(*argv + 2) < 0)
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
            case 'H':
                if (set_header_edit(*argv + 2) < 0)
                    return -1;
                logging(LOG_INFO, "Header edit: [%s]", *argv + 2);
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
            logging(LOG_ERR, "%s: failed.", *argv);
            rval = -1;
            break;
        }
        vname = NULL;
    }
    return rval;
}


static void
print_version(FILE *fp)
{
    fprintf(fp,
            "Versions:\n"
            "    main: %s\n"
            "    libgtool3: %s\n"
            "    CMOR: %d.%d.%d\n"
            "    netcdf library: %s\n"
            "\n",
            mipconv_version(), GT3_version(),
            CMOR_VERSION_MAJOR, CMOR_VERSION_MINOR, CMOR_VERSION_PATCH,
            nc_inq_libvers());
}


static void
usage(void)
{
    const char *usage_message =
        "Usage: " PROGNAME
        " [options] user_input.json CMIP6_*.json :vname [voption] files...\n"
        "\n"
        "Options:\n"
        "    -3           use netCDF3 format.\n"
        "    -M           specify a directory which contains CMIP6_*.json.\n"
        "    -d DIR       specify output directory.\n"
        "    -f conffile  specify global attribute file.\n"
        "    -g mapping   specify grid mapping.\n"
        "                 (\"rotated_pole\", \"bipolar\", \"tripolar\")\n"
        "    -m mode      specify writing mode(\"preserve\" or \"replace\").\n"
        "                 (default: \"replace\")\n"
        "    -s           safe mode.\n"
        "    -v           verbose mode.\n"
        "    -h           print this message.\n"
        "\n";
    const char *usage_message2 =
        "Var Options:\n"
        "    =c...        specify a filename which contains comments.\n"
        "    =e...        specify expression.\n"
        "    =p...        specify 'up' or 'down'.\n"
        "    =t...        specify Data No.(time slice) list.\n"
        "    =u...        specify unit.\n"
        "    =z...        specify z-level slice.\n"
        "    =H...        specify header editing.\n"
        "\n";
    const char *examples =
        "Examples:\n"
        "  $ ./mipconv -M ../Tables user_input.json CMIP6_Amon.json :ps y*/Ps\n"
        "  $ ./mipconv -M ../Tables user_input.json CMIP6_Amon.json :rlut =pup y*/olr\n"
        "  $ ./mipconv -M ../Tables user_input.json CMIP6_Amon.json :cl y*/cldfrc :ps y*/Ps\n"
        "\n";

    print_version(stderr);
    fprintf(stderr, usage_message);
    fprintf(stderr, usage_message2);
    fprintf(stderr, examples);
}


int
main(int argc, char **argv)
{
    int ch, rval = 0;
    FILE *fp = NULL;
    int ntables = 1;
    char *mipdir = NULL;
    char *outputdir = NULL;

    open_logging(stderr, PROGNAME);
    GT3_setProgname(PROGNAME);

    while ((ch = getopt(argc, argv, "34M:d:f:g:m:svh")) != -1)
        switch (ch) {
        case '3':
            use_netcdf(3);
            break;
        case '4':
            use_netcdf(4);
            break;
        case 'M':
            if ((mipdir = strdup(optarg)) == NULL) {
                logging(LOG_SYSERR, optarg);
                exit(1);
            }
            break;
        case 'd':
            if ((outputdir = strdup(optarg)) == NULL) {
                logging(LOG_SYSERR, optarg);
                exit(1);
            }
            break;
        case 'f':
            if ((fp = fopen(optarg, "r")) == NULL) {
                logging(LOG_SYSERR, optarg);
                exit(1);
            }
            if (read_config(fp) < 0) {
                logging(LOG_ERR, "in configuration file (%s).", optarg);
                exit(1);
            }
            fclose(fp);
            break;
        case 'g':
            if (set_grid_mapping(optarg) < 0) {
                logging(LOG_ERR, "%s: unsupported mapping.", optarg);
                exit(1);
            }
            ntables++;
            break;
        case 'm':
            if (set_writing_mode(optarg) < 0) {
                logging(LOG_ERR, "%s: unknown mode.", optarg);
                exit(1);
            }
            break;
        case 's':
            set_safe_open();
            break;
        case 'v':
            print_version(stderr);
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
    if (argc < ntables + 1) {
        usage();
        exit(1);
    }

    /*
     * setup CMOR.
     */
    if (setup(mipdir, outputdir, *argv) < 0)
        exit(1);
    argc--;
    argv++;

    /*
     * load MIP tables.
     */
    if (load_normal_table(*argv) < 0
        || (ntables > 1 && load_grid_table(*(argv + 1)) < 0))
        exit(1);
    switch_to_normal_table();

    argv += ntables;
    argc -= ntables;
    rval = process_args(argc, argv);
    cmor_close();
    logging(LOG_INFO, rval == 0 ? "SUCCESSFUL END" : "ABNORMAL END");
    return rval < 0 ? 1 : 0;
}
