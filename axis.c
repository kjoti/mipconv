/*
 *  axis.c
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gtool3.h"
#include "logging.h"
#include "cmor.h"
#include "cmor_supp.h"
#include "internal.h"


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


/*
 *  name: dimension name defined in MIP-table.
 *  orig_unit: unit of values (It may differ from MIP-table)
 */
static int
get_axis_by_values(const char *name,
                   const char *orig_unit,
                   const double *values,
                   const double *bnds,
                   int len)
{
    int bnd_ndim = bnds ? 1 : 0;
    int newid;

    if (cmor_axis(&newid, (char *)name, (char *)orig_unit, len,
                  (double *)values, 'd',
                  (double *)bnds, bnd_ndim, NULL) < 0) {

        logging(LOG_ERR, "cmor_axis() failed");
        return -1;
    }
    assert(newid >= 0);
    return newid;
}


int
dummy_dimname(const char *name)
{
    char *dummy[] = {
        "",
        "SFC1",
        "NUMBER1000"
    };
    int i;

    for (i = 0; i < sizeof dummy / sizeof(char *); i++)
        if (strcmp(name, dummy[i]) == 0)
            return 1;
    return 0;
}


/*
 * get_axis_ids() returns axis-IDs for given GTOOL3 AITM[1-3].
 *
 * ids: axis IDs
 * nids: the number of returned IDs (typically 0 or 1).
 *
 * Example:
 *    if (get_axis_ids(ids, &nids, "GLON128", 1, 128, vdef) < 0) {
 *    }
 */
int
get_axis_ids(int *ids, int *nids,
             const char *aitm, int astr, int aend,
             struct sequence *zslice,
             const cmor_var_def_t *vdef)
{
    GT3_Dim *dim;
    GT3_DimBound *bnd = NULL;
    double *bounds = NULL;
    double *values;
    int dimlen;
    cmor_axis_def_t *adef;
    int rval = -1;
    int axisid;

    *nids = 0;
    if (aend - astr <= 0 && dummy_dimname(aitm)) {
        logging(LOG_INFO, "skip empty dim (%s)", aitm);
        return 0;
    }

    if ((dim = GT3_getDim(aitm)) == NULL) {
        GT3_printErrorMessages(stderr);
        return -1;
    }

    /* check special axis. */
    if (strcmp(aitm, "tauplev") == 0) {
        assert(!"not yet implemented");
    } else {
        adef = lookup_axisdef_in_vardef(dim->title, vdef);
        if (!adef) {
            logging(LOG_ERR, "No such axis: %s", dim->title);
            goto finish;
        }

        if (adef->must_have_bounds) {
            if ((bnd = GT3_getDimBound(aitm)) == NULL) {
                char newname[17];

                snprintf(newname, sizeof newname, "%s.M", aitm);
                if ((bnd = load_as_dimbound(newname)) == NULL)
                    goto finish;
            }
            bounds = bnd->bnd + astr - 1;
        }

        if (zslice) {
            /*
             * z-level slicing.
             */
            assert(!"not yet implimented");
            values = NULL;
            dimlen = 0;
        } else {
            values = dim->values + astr - 1;
            dimlen = aend - astr + 1;
        }

        if ((axisid = get_axis_by_values(adef->id,
                                         dim->unit,
                                         values,
                                         bounds,
                                         dimlen)) < 0)
            goto finish;

        ids[0] = axisid;
        *nids = 1;
        rval = 0;
    }
finish:
    GT3_freeDim(dim);
    GT3_freeDimBound(bnd);
    return rval;
}


#ifdef TEST_MAIN2
int
test_axis(void)
{
    {
        GT3_DimBound *bnd;
        int i;

        bnd = load_as_dimbound("CSIG56.M");
        assert(bnd->len == 57);
        assert(bnd->bnd[0] == 1.);
        assert(bnd->bnd[56] == 0.);
        for (i = 1; i < 57; i++)
            assert(bnd->bnd[i] < bnd->bnd[i-1]);
    }

    {
        int id;
        double lat[] = {67.5, 22.5, -22.5, -67.5 };
        double bnd[] = {90., 45., 0., -45., -90.};
        cmor_axis_t *axis;

        id = get_axis_by_values("latitude", "degrees_north",
                                lat, bnd, 4);
        assert(id >= 0);

        axis = cmor_axes + id;
        assert(axis->axis == 'Y');
        assert(axis->bounds[0] == -90.);
        assert(axis->bounds[1] == -45.);
        assert(axis->bounds[2] == -45.);
        assert(axis->bounds[3] == 0.);
        assert(axis->bounds[7] == 90.);
    }

    printf("test_axis(): DONE\n");
    return 0;
}
#endif /* TEST_MAIN2 */
