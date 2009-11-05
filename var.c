/*
 *  var.c
 */
#include <stdlib.h>

#include "logging.h"

#include "gtool3.h"
#include "var.h"


static void
init_var(myvar_t *var)
{
    var->rank = 0;
    var->data = NULL;
    var->typecode = '?';
    var->title = NULL;
    var->unit = NULL;
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
read_var(myvar_t *var, GT3_Varbuf *vbuf)
{
    int z, nxy, nz;
    float *vptr;
    static int cnt = 0;


    nxy = vbuf->dimlen[0] * vbuf->dimlen[1];
    nz = vbuf->dimlen[2];

    for (z = 0, vptr = var->data; z < nz; z++, vptr += nxy)
        if (GT3_readVarZ(vbuf, z) < 0
            || GT3_copyVarFloat(vptr, nxy, vbuf, 0, 1) < 0) {
            GT3_printErrorMessages(stderr);
            return -1;
        }

    var->timebnd[0] = 30. * cnt;
    var->timebnd[1] = 30. * (cnt + 1);
    var->time = .5 * (var->timebnd[0] + var->timebnd[1]);
    cnt++;
    return 0;
}
