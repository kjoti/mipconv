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
 * Model parameters.
 */
double ocean_sigma_bottom = 50.; /* ZBOT [m] */


struct param_entry {
    const char *key;
    char type;
    void *ptr;
};

static struct param_entry param_tab[] = {
    { "ocean_sigma_bottom", 'd', &ocean_sigma_bottom },
    { NULL }
};


static int cmor_mode = CMOR_REPLACE;


int
set_writing_mode(const char *str)
{
    struct { const char *key; int value; } tab[] = {
        { "preserve", CMOR_PRESERVE },
        { "append",   CMOR_APPEND },
        { "replace",  CMOR_REPLACE }
    };
    int i;

    for (i = 0; i < sizeof tab / sizeof tab[0]; i++)
        if (strcmp(str, tab[i].key) == 0) {
            cmor_mode = tab[i].value;
            return 0;
        }
    return -1;
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
    char value[CMOR_MAX_STRING];
    int rc;

    rc = cmor_setup((char *)mipdir, &cmor_mode, NULL, NULL, NULL, NULL);
    if (rc != 0) {
        logging(LOG_ERR, "cmor_setup() failed.");
        return -1;
    }

    rc = cmor_dataset_json((char *)userconf);
    if (rc != 0) {
        logging(LOG_ERR, "cmor_dataset_json(): failed.");
        return -1;
    }

    /*
     * Overwrite `outpath` if `outdir` is given.
     */
    if (outdir) {
        strlcpy(cmor_current_dataset.outpath, outdir, CMOR_MAX_STRING);
        if (strcmp(cmor_current_dataset.outpath, outdir) != 0) {
            logging(LOG_ERR, "%s: outdir is too long.", outdir);
            return -1;
        }
        if (cmor_outpath_exist(cmor_current_dataset.outpath) != 0)
            return -1;
    }
    logging(LOG_INFO, "output directory: (%s)", cmor_current_dataset.outpath);

    /*
     * Set the calendar from CMOR dataset (via `userconf`).
     */
    if (cmor_get_cur_dataset_attribute(GLOBAL_ATT_CALENDAR, value) != 0)
        return -1;
    if (set_calendar_by_name(value) < 0) {
        logging(LOG_ERR, "%s: Unknown calendar name.", value);
        return -1;
    }
    logging(LOG_INFO, "caledar: %s", value);

    /*
     * Set the basetime from CMOR dataset (via `userconf`).
     */
    if (cmor_has_cur_dataset_attribute("_basetime") == 0) {
        cmor_get_cur_dataset_attribute("_basetime", value);

        if (set_basetime(value) < 0) {
            logging(LOG_ERR, "%s: Invalid basetime.", value);
            return -1;
        }
        logging(LOG_INFO, "basetime: %s", value);
    }

    logging_current_attributes();
    return 0;
}
