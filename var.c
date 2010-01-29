/*
 *  var.c
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

    /* trimming */
    for (i = 0; i < ndim; i++) {
        if (dimlen[i] <= 1)
            break;
        nelems *= dimlen[i];
    }
    ndim = i;
    if (ndim == 0) {
        logging(LOG_ERR, "empty variable");
        return -1;
    }

    if ((temp = malloc(sizeof(float) * nelems)) == NULL) {
        logging(LOG_SYSERR, "resize_var(): ");
        return -1;
    }
    var->rank = ndim;
    for (i = 0; i < MAX_NDIM; i++)
        var->dimlen[i] = (i < ndim) ? dimlen[i] : 1;
    var->data = temp;
    var->typecode = 'f';

    return 0;
}


int
read_var(myvar_t *var, GT3_Varbuf *vbuf, struct sequence *zseq)
{
    int nxy, nz, n;
    float *vptr;

    nxy = var->dimlen[0] * var->dimlen[1];
    nz = var->dimlen[2];
    assert(nxy == vbuf->dimlen[0] * vbuf->dimlen[1]);

    for (vptr = var->data; nz > 0; nz--, vptr += nxy) {
        if (zseq) {
            if (nextSeq(zseq) != 1)
                logging(LOG_WARN, "Invalid slicing");

            n = zseq->curr - 1;
        } else
            n = var->dimlen[2] - nz;

        if (GT3_readVarZ(vbuf, n) < 0
            || GT3_copyVarFloat(vptr, nxy, vbuf, 0, 1) < 0) {
            GT3_printErrorMessages(stderr);
            return -1;
        }
    }
    return 0;
}
