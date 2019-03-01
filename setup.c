/*
 * setup.c -- setup CMOR
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


/*
 * model parameters.
 */
static char *basetime = NULL;
double ocean_sigma_bottom = 50.; /* ZBOT [m] */


struct param_entry {
    const char *key;
    char type;
    void *ptr;
};

static struct param_entry param_tab[] = {
    { "basetime", 'c', &basetime },
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


void
logging_current_attributes(void)
{
    const char *keys[] = {
        GLOBAL_ATT_EXPERIMENTID,
        GLOBAL_ATT_REALIZATION,
        GLOBAL_ATT_INITIA_IDX,
        GLOBAL_ATT_PHYSICS_IDX,
        GLOBAL_ATT_FORCING_IDX,
    };
    char value[CMOR_MAX_STRING];
    int i;

    for (i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        if (cmor_get_cur_dataset_attribute((char *)keys[i], value) != 0)
            strlcpy(value, "(nil)", sizeof value);

        logging(LOG_INFO, "%s = %s", keys[i], value);
    }
}


/*
 * mipdir: A directory which contains MIP Tables (such as CMIP6_CV.json,
 *        CMIP6_Amon.json and so on).
 * outdir: A directory in which  output files are written.
 * userconf: A JSON file which specifies user specific meta data
 *           (e.g., user_input.json).
 */
int
setup(const char *mipdir, const char *outdir, const char *userconf)
{
    int status, action;

    if (netcdf_version == 0)
        netcdf_version = default_version();

    action = setupmode_in_cmor();

    logging(LOG_INFO, "use netCDF%d format.", netcdf_version);
    status = cmor_setup((char *)mipdir, &action, NULL, NULL, NULL, NULL);
    if (status != 0) {
        logging(LOG_ERR, "cmor_setup() failed.");
        return -1;
    }

    strlcpy(cmor_current_dataset.outpath,
            outdir ? outdir : "./",
            sizeof cmor_current_dataset.outpath);

    status = cmor_dataset_json((char *)userconf);
    if (status != 0) {
        logging(LOG_ERR, "cmor_dataset_json(): failed.");
        return -1;
    }

    if (basetime && set_basetime(basetime) < 0) {
        logging(LOG_ERR, "%s: invalid basetime.", basetime);
        return -1;
    }

    logging_current_attributes();
    return 0;
}
