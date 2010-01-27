/*
 *  setup.c -- setup of data converter for CMIP5.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "cmor.h"

#include "internal.h"

static char *outputdir = NULL;


/*
 *  global parameters for dataset.
 */
static char *experiment_id = "pre-industrial control";
static char *institution = "CCSR+NIES+FRCGC";
static char *source = "MIROC4.0";
static char *calendar = "noleap";
static int realization = 1;
static char *contact = "emori@nies.go.jp";
static char *history = "";
static char *comment = "this is a sample data";
static char *references = "in print";
static char *model_id = "MIROC";
static char *forcing = "RCP";
static int initialization_method = 0;
static int physics_version = 0;
static char *institute_id = "CCSR+NIES+FRCGC";

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


int
set_parameter(const char *key, const char *value)
{
    struct param_entry *ent = param_tab;


    for (ent = param_tab; ent->key; ent++) {
        if (strcmp(ent->key, key) == 0) {
            switch (ent->type) {
            case 'c':
                *ent->ptr = strdup(value);
                logging(LOG_INFO, "[%s] = %s", key, value);
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
set_outdir(const char *dir)
{
    if (dir) {
        free(outputdir);
        if ((outputdir = strdup(dir)) == NULL) {
            logging(LOG_SYSERR, NULL);
            return -1;
        }
    }
    return 0;
}


/*
 *
 */
int
read_config(FILE *fp)
{
    char aline[4096], key[32], *ptr;
    size_t len;

    while (read_logicline(aline, sizeof aline, fp) > 0) {
        /* skip a comment line */
        if (aline[0] == '#')
            continue;

        for (ptr = aline;
             *ptr != '\0' && *ptr != ' ' && *ptr != '\t';
             ptr++)
            ;

        len = ptr - aline;
        if (len > sizeof key - 1)
            len = sizeof key - 1;
        memcpy(key, aline, len);
        key[len] = '\0';

        while (*ptr == ' ' || *ptr == '\t')
            ptr++;

        set_parameter(key, ptr);
    }
    return 0;
}


void
setup(void)
{
    int status;
    int netcdf = CMOR_REPLACE_3;

    status = cmor_setup(NULL, &netcdf, NULL, NULL, "cmor.log", NULL);
    if (status != 0) {
        logging(LOG_ERR, "cmor_setup() failed");
        exit(1);
    }

    status = cmor_dataset(
        outputdir ? outputdir : "/tmp",
        experiment_id,
        institution,
        source,
        calendar,
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
        institute_id);

    if (status != 0) {
        logging(LOG_ERR, "cmor_dataset(): failed");
        exit(1);
    }

    set_calendar_by_name(calendar);
    set_origin_year(origin_year);
}
