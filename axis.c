/*
 *  axis.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gtool3.h"
#include "logging.h"

#include "cmor.h"

/*
 *  make GT3_DimBound * from GT3_Dim *.
 */
static GT3_DimBound *
get_dimbound_from_gt3dim(const GT3_Dim *dim)
{
    GT3_DimBound *bnd;
    int i;

    if (!dim)
        return NULL;

    if ((bnd = malloc(sizeof(GT3_DimBound))) == NULL
        || (bnd->bnd = malloc(sizeof(double) * dim->len)) == NULL) {
        logging(LOG_SYSERR, NULL);
        return NULL;
    }

    bnd->name = strdup(dim->name);
    for (i = 0; i < dim->len; i++)
        bnd->bnd[i] = dim->values[i];

    bnd->len = dim->len;
    bnd->len_orig = dim->len - 1;

    return bnd;
}


/*
 *  get GT3_DimBound from mid-level axis-file.
 */
static GT3_DimBound *
load_as_dimbound(const char *name)
{
    GT3_Dim *dim;
    GT3_DimBound *bnd;


    if ((dim = GT3_loadDim(name)) == NULL) {
        GT3_printErrorMessages(stderr);
        return NULL;
    }

    bnd = get_dimbound_from_gt3dim(dim);
    GT3_freeDim(dim);
    return bnd;
}


static int
get_axis_by_gt3dim(int *axisid,
                   const GT3_Dim *dim, const GT3_DimBound *bnd,
                   int offset, int len)
{
    int newid;
    double *bnd_value;
    int bnd_ndim;

    if (bnd) {
        bnd_value = bnd->bnd;
        bnd_ndim = 1;
    } else {
        bnd_value = NULL;
        bnd_ndim = 0;
    }

    if (len == 0)
        len = dim->len - dim->cyclic - offset;

    if (cmor_axis(&newid, dim->title, dim->unit, len,
                  dim->values + offset, 'd',
                  bnd_value + offset, bnd_ndim, NULL) < 0) {

        logging(LOG_ERR, "cmor_axis() failed");
        return -1;
    }

    *axisid = newid;
    return 0;
}


/*
 *  EXAMPLE:
 *   axisid = get_axis_by_gt3anme("GGLA64", 1, 64);
 */
int
get_axis_by_gt3name(const char *name, int astr, int aend)
{
    GT3_Dim *dim;
    GT3_DimBound *bnd = NULL;
    int axisid = -1;
    int err = 0;

    if ((dim = GT3_getDim(name)) == NULL) {
        GT3_printErrorMessages(stderr);
        return -1;
    }

    if ((bnd = GT3_getDimBound(name)) == NULL) {
        char newname[17];

        snprintf(newname, sizeof newname, "%s.M", name);

        if ((bnd = load_as_dimbound(newname)) == NULL)
            err = 1;
    }

    if (!err)
        get_axis_by_gt3dim(&axisid, dim, bnd, astr - 1, aend - astr + 1);

    GT3_freeDim(dim);
    GT3_freeDimBound(bnd);
    return axisid;
}


int
get_axis_by_gtool3(const GT3_HEADER *head, int idx)
{
    static const char *aitm[] = { "AITM1", "AITM2", "AITM3" };
    static const char *astr[] = { "ASTR1", "ASTR2", "ASTR3" };
    static const char *aend[] = { "AEND1", "AEND2", "AEND3" };
    char name[17];
    int istr, iend;

    if (GT3_copyHeaderItem(name, sizeof name, head, aitm[idx]) == NULL
        || GT3_decodeHeaderInt(&istr, head, astr[idx]) < 0
        || GT3_decodeHeaderInt(&iend, head, aend[idx]) < 0)
        return -1;

    return get_axis_by_gt3name(name, istr, iend);
}


#ifdef TEST_MAIN2
int
test_axis(void)
{
    return 0;
}
#endif /* TEST_MAIN2 */
