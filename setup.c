/*
 * setup.c -- setup of data converter for CMIP5.
 */
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "cmor.h"

#include "internal.h"
#include "myutils.h"

static char *outputdir = NULL;


/*
 * global parameters for dataset.
 */
static char *experiment_id = "pre-industrial control";
static char *institution = "CCSR+NIES+FRCGC";
static char *source = "MIROC4.0";
static char *calendar = NULL;
static int realization = 1;
static char *contact = NULL;
static char *history = NULL;
static char *comment = NULL;
static char *references = NULL;
static char *model_id = "MIROC";
static char *forcing = "N/A";
static int initialization_method = 0;
static int physics_version = 0;
static char *institute_id = "CCSR+NIES+FRCGC";
static char *parent_experiment_id = "N/A";
static double zero = 0.;
static double *branch_time = &zero;


static int origin_year = 1;

struct param_entry {
    const char *key;
    char type;
    union {
        char **c;
        int *i;
    } ptr;
};

static struct param_entry param_tab[] = {
    { "experiment_id", 'c', .ptr.c = &experiment_id },
    { "institution",   'c', .ptr.c = &institution },
    { "source",        'c', .ptr.c = &source },
    { "calendar",      'c', .ptr.c = &calendar },
    { "realization",   'i', .ptr.i = &realization },
    { "contact",       'c', .ptr.c = &contact },
    { "history",       'c', .ptr.c = &history },
    { "comment",       'c', .ptr.c = &comment },
    { "references",    'c', .ptr.c = &references },
    { "model_id",      'c', .ptr.c = &model_id },
    { "forcing",       'c', .ptr.c = &forcing },
    { "initialization_method", 'i', .ptr.i = &initialization_method },
    { "physics_version", 'i', .ptr.i = &physics_version },
    { "institute_id",  'c', .ptr.c = &institute_id },
    { "origin_year",   'i', .ptr.i = &origin_year },
    { NULL }
};


static int
set_parameter(const char *key, const char *value)
{
    struct param_entry *ent;

    for (ent = param_tab; ent->key; ent++) {
        if (strcmp(ent->key, key) == 0) {
            logging(LOG_INFO, "[%s] = %s", key, value);
            switch (ent->type) {
            case 'c':
                *ent->ptr.c = strdup(value);
                break;
            case 'i':
                *ent->ptr.i = (int)strtol(value, NULL, 0);
                break;
            default:
                break;
            }
            return 0;
        }
    }
    logging(LOG_WARN, "[%s]: No such parameter", key);
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
        logging(LOG_ERR, "%s: Not a directory", path);
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
    int action = CMOR_REPLACE_3;
    int message = CMOR_NORMAL;

    status = cmor_setup(NULL, &action, &message, NULL, NULL, NULL);
    if (status != 0) {
        logging(LOG_ERR, "cmor_setup() failed");
        return -1;
    }

    status = cmor_dataset(
        outputdir ? outputdir : "/tmp",
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
        branch_time);

    if (status != 0) {
        logging(LOG_ERR, "cmor_dataset(): failed");
        return -1;
    }

    if (calendar && set_calendar_by_name(calendar) < 0) {
        logging(LOG_ERR, "%s: unknown calendar");
        return -1;
    }

    set_origin_year(origin_year);
    return 0;
}


#ifdef TEST_MAIN2
#endif
