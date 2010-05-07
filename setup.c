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
    void **ptr;
};

static struct param_entry param_tab[] = {
    { "experiment_id", 'c', (void **)&experiment_id },
    { "institution",   'c', (void **)&institution },
    { "source",        'c', (void **)&source },
    { "calendar",      'c', (void **)&calendar },
    { "realization",   'i', (void **)&realization },
    { "contact",       'c', (void **)&contact },
    { "history",       'c', (void **)&history },
    { "comment",       'c', (void **)&comment },
    { "references",    'c', (void **)&references },
    { "model_id",      'c', (void **)&model_id },
    { "forcing",       'c', (void **)&forcing },
    { "initialization_method", 'i', (void **)&initialization_method },
    { "physics_version", 'i', (void **)&physics_version },
    { "institute_id",  'c', (void **)&institute_id },
    { "origin_year",   'i', (void **)&origin_year },
    { NULL, ' ', NULL }
};


static int
set_parameter(const char *key, const char *value)
{
    struct param_entry *ent = param_tab;


    for (ent = param_tab; ent->key; ent++) {
        if (strcmp(ent->key, key) == 0) {
            logging(LOG_INFO, "[%s] = %s", key, value);
            switch (ent->type) {
            case 'c':
                *ent->ptr = strdup(value);
                break;
            case 'i':
                *(int *)ent->ptr = (int)strtol(value, NULL, 0);
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
    char aline[4096], key[32], *ptr;
    size_t kwlen;
    int rval = 0;

    while (!feof(fp)) {
        read_logicline(aline, sizeof aline, fp);

        /* skip a comment line or a blank line */
        if (aline[0] == '#' || aline[0] == '\0')
            continue;

        for (ptr = aline;
             *ptr != '\0' && *ptr != ' ' && *ptr != '\t';
             ptr++)
            ;

        kwlen = ptr - aline;
        if (kwlen > sizeof key - 1)
            kwlen = sizeof key - 1;
        memcpy(key, aline, kwlen);
        key[kwlen] = '\0';

        while (*ptr == ' ' || *ptr == '\t')
            ptr++;

        if (set_parameter(key, ptr) < 0)
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
