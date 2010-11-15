/*
 * var.c
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "gtool3.h"

#include "internal.h"
#include "var.h"


static void
init_var(myvar_t *var)
{
    memset(var, 0, sizeof(myvar_t));
}


myvar_t *
new_var(void)
{
    myvar_t *temp;

    if ((temp = malloc(sizeof(myvar_t))) == NULL) {
        logging(LOG_SYSERR, "new_var(): ");
        return NULL;
    }
    init_var(temp);
    return temp;
}


void
free_var(myvar_t *var)
{
    if (var) {
        free(var->data);
        free(var->title);
        free(var->unit);
        init_var(var);
    }
}


int
resize_var(myvar_t *var, const int *dimlen, int ndim)
{
    int i;
    size_t nelems = 1;
    float *temp;

    for (i = 0; i < MAX_NDIM; i++)
        var->dimlen[i] = 1;

    for (i = 0; i < ndim; i++) {
        var->dimlen[i] = dimlen[i];
        nelems *= dimlen[i];
    }
    if ((temp = malloc(sizeof(float) * nelems)) == NULL) {
        logging(LOG_SYSERR, "resize_var(): ");
        return -1;
    }
    var->rank = ndim;
    var->data = temp;
    var->nelems = nelems;
    var->typecode = 'f';

    return 0;
}


int
read_var(myvar_t *var, GT3_Varbuf *vbuf, struct sequence *zseq)
{
    int nxy, nz, n, z;
    float *vptr;

    nxy = var->dimlen[0] * var->dimlen[1];
    nz = var->dimlen[2];

    for (vptr = var->data, n = 0; n < nz; n++, vptr += nxy) {
        if (zseq) {
            if (nextSeq(zseq) != 1)
                logging(LOG_WARN, "Invalid slicing.");

            z = zseq->curr - 1;
        } else
            z = n;

        if (GT3_readVarZ(vbuf, z) < 0
            || GT3_copyVarFloat(vptr, nxy, vbuf, 0, 1) < 0) {
            GT3_printErrorMessages(stderr);
            return -1;
        }
    }
    return 0;
}
