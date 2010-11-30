/*
 * setup.c -- setup of data converter for CMIP5.
 */
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "cmor.h"

#include "internal.h"
#include "myutils.h"

#include "netcdf.h"

static char *outputdir = NULL;

/*
 * global attributes for dataset.
 */
static char *experiment_id = "pre-industrial control";
static char *institution = "MIROC(AORI, NIES and JAMSTEC)";
static char *source = "MIROC4h 2009";
static char *calendar = NULL;
static int realization = 1;
static char *contact = "anonymous";
static char *history = NULL;
static char *comment = NULL;
static char *references = NULL;
static char *model_id = "MIROC4h";
static char *forcing = "N/A";
static int initialization_method = 1;
static int physics_version = 1;
static char *institute_id = "MIROC";
static char *parent_experiment_id = "N/A";
static char *parent_experiment_rip = "N/A";
static double branch_time = 0.;

static int origin_year = 1;

/*
 * model parameters.
 */
double ocean_sigma_bottom = 38.; /* ZBOT [m] */


struct param_entry {
    const char *key;
    char type;
    void *ptr;
};

static struct param_entry param_tab[] = {
    { "experiment_id", 'c', &experiment_id },
    { "institution",   'c', &institution },
    { "source",        'c', &source },
    { "calendar",      'c', &calendar },
    { "realization",   'i', &realization },
    { "contact",       'c', &contact },
    { "history",       'c', &history },
    { "comment",       'c', &comment },
    { "references",    'c', &references },
    { "model_id",      'c', &model_id },
    { "forcing",       'c', &forcing },
    { "initialization_method", 'i', &initialization_method },
    { "physics_version", 'i', &physics_version },
    { "institute_id",  'c', &institute_id },
    { "parent_experiment_id", 'c', &parent_experiment_id },
    { "branch_time",   'd', &branch_time },
    { "parent_experiment_rip", 'c', &parent_experiment_rip },
    { "origin_year",   'i', &origin_year },
    /* model parameters */
    { "ocean_sigma_bottom", 'd', &ocean_sigma_bottom },
    { NULL }
};


/*
 * cmor_setup mode.
 */
static int netcdf_version = 0;

enum {
    MODE_PRESERVE,
    MODE_APPEND,
    MODE_REPLACE
};
static int writing_mode = MODE_REPLACE;


static int
is_valid_version(int major)
{
    return major >= 3 && major <= 4;
}


static int
linked_netcdf_version(void)
{
    char *p, buf[64];
    int major;

    snprintf(buf, sizeof buf, "%s", nc_inq_libvers());
    for (p = buf; !isdigit(*p) && *p != '\0'; ++p)
        ;

    major = strtol(p, NULL, 10);
    if (!is_valid_version(major)) {
        logging(LOG_WARN, "%s: unexpected netcdf version.", buf);
        major = 3;
    }
    return major;
}


static int
default_version(void)
{
    /* return 3; */
    return linked_netcdf_version();
}


int
use_netcdf(int v)
{
    int version = linked_netcdf_version();

    if (version == 3) {
        netcdf_version = 3;
        if (v != 3)
            logging(LOG_WARN, "netcdef version must be 3.");
    }

    if (version == 4) {
        if (!(v >= 3 && v <= 4)) {
            logging(LOG_WARN, "netcdef version must be 3 or 4.");
            v = 4;
        }
        netcdf_version = v;
    }
    return 0;
}


int
set_writing_mode(const char *str)
{
    struct { const char *key; int value; } tab[] = {
        { "preserve", MODE_PRESERVE },
        { "append", MODE_APPEND },
        { "replace", MODE_REPLACE }
    };
    int i;
    int mode = -1;

    for (i = 0; i < sizeof tab / sizeof tab[0]; i++)
        if (strcmp(str, tab[i].key) == 0) {
            mode = tab[i].value;
            break;
        }

    if (mode == -1)
        return -1;

    writing_mode = mode;
    return 0;
}


static int
setupmode_in_cmor(void)
{
    int rval = CMOR_REPLACE_3;

    if (netcdf_version == 3) {
        int tab[] = { CMOR_PRESERVE_3, CMOR_APPEND_3, CMOR_REPLACE_3 };

        rval = tab[writing_mode];
    }

    if (netcdf_version == 4) {
        int tab[] = { CMOR_PRESERVE_4, CMOR_APPEND_4, CMOR_REPLACE_4 };

        rval = tab[writing_mode];
    }
    return rval;
}


static int
set_parameter(const char *key, const char *value)
{
    struct param_entry *ent;

    for (ent = param_tab; ent->key; ent++) {
        if (strcmp(ent->key, key) == 0) {
            logging(LOG_INFO, "[%s] = %s", key, value);
            switch (ent->type) {
            case 'c':
                *(char **)ent->ptr = strdup(value);
                break;
            case 'i':
                *(int *)ent->ptr = (int)strtol(value, NULL, 0);
                break;
            case 'd':
                *(double *)ent->ptr = strtod(value, NULL);
                break;
            default:
                break;
            }
            return 0;
        }
    }
    logging(LOG_WARN, "[%s]: No such parameter.", key);
    return -1;  /* not found */
}


int
set_outdir(const char *path)
{
    struct stat sb;

    if (stat(path, &sb) < 0) {
        logging(LOG_SYSERR, path);
        return -1;
    }
    if (!S_ISDIR(sb.st_mode)) {
        logging(LOG_ERR, "%s: Not a directory.", path);
        return -2;
    }

    free(outputdir);
    if ((outputdir = strdup(path)) == NULL) {
        logging(LOG_SYSERR, NULL);
        return -1;
    }
    return 0;
}


int
read_config(FILE *fp)
{
    char aline[4096], key[32], *value;
    int rval = 0;

    while (!feof(fp)) {
        read_logicline(aline, sizeof aline, fp);

        /* skip a comment line or a blank line */
        if (aline[0] == '#' || aline[0] == '\0')
            continue;

        value = split2(key, sizeof key, aline, " \t");
        if (set_parameter(key, value) < 0)
            rval = -1;
    }
    return rval;
}


int
setup(void)
{
    int status;
    int message = CMOR_NORMAL;
    /* int action = CMOR_REPLACE_4; */
    int action;

    if (netcdf_version == 0)
        netcdf_version = default_version();

    action = setupmode_in_cmor();

    logging(LOG_INFO, "use netCDF%d format.", netcdf_version);
    status = cmor_setup(NULL, &action, &message, NULL, NULL, NULL);
    if (status != 0) {
        logging(LOG_ERR, "cmor_setup() failed.");
        return -1;
    }

    status = cmor_dataset(
        outputdir ? outputdir : "./",
        experiment_id,
        institution,
        source,
        calendar ? calendar : "standard",
        realization,
        contact,
        history,
        comment,
        references,
        0,
        0,
        NULL,
        model_id,
        forcing,
        initialization_method,
        physics_version,
        institute_id,
        parent_experiment_id,
        &branch_time,
        parent_experiment_rip);

    if (status != 0) {
        logging(LOG_ERR, "cmor_dataset(): failed.");
        return -1;
    }

    if (calendar && set_calendar_by_name(calendar) < 0) {
        logging(LOG_ERR, "%s: unknown calendar.", calendar);
        return -1;
    }

    set_origin_year(origin_year);
    return 0;
}


#ifdef TEST_MAIN2
#endif
