/*
 * axis.c
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
 * make GT3_DimBound * from GT3_Dim *.
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
 * get GT3_DimBound from mid-level axis-file.
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
 * name: dimension name defined in MIP-table.
 * orig_unit: unit of values (It may differ from MIP-table)
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


/*
 *  In the case of 'index_only: yes'.
 */
static int
get_axis_by_index(const char *name, int len)
{
#if 0
    int newid;
    int *idx, i;

    if ((idx = malloc(sizeof(int) * len)) == NULL) {
        logging(LOG_SYSERR, NULL);
        return -1;
    }
    for (i = 0; i < len; i++)
        idx[i] = i;

    if (cmor_axis(&newid, (char *)name, " ", len,
                  idx, 'i',
                  NULL, 0, NULL) < 0) {

        logging(LOG_ERR, "cmor_axis() failed");
        return -1;
    }
    free(idx);
    return newid;
#else
    return get_axis_by_values(name, " ", NULL, NULL, len);
#endif
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


static double *
slice_gtdim(int *len,
            const GT3_Dim *dim, int astr, int aend,
            struct sequence *zseq)
{
    double *values;
    int k, num, cnt;

    reinitSeq(zseq, astr, aend);
    num = countSeq(zseq);
    if ((values = malloc(sizeof(double) * num)) == NULL) {
        logging(LOG_SYSERR, NULL);
        return NULL;
    }

    cnt = 0;
    while (nextSeq(zseq) > 0) {
        k = zseq->curr - astr;
        if (zseq->curr < 0)
            k += aend + 1;

        if (k >= 0 && k < dim->len)
            values[cnt++] = dim->values[k];
    }
    *len = cnt;
    return values;
}


static int
get_axisid(const GT3_Dim *dim,
           int astr, int aend,
           const cmor_axis_def_t *adef,
           struct sequence *zslice)
{
    double *values = NULL;
    double *bounds = NULL;
    int dimlen;
    GT3_DimBound *bnd = NULL;
    int axisid = -1;

    assert(dim->len >= 1 + aend - astr);

    if (adef->must_have_bounds) {
        if ((bnd = GT3_getDimBound(dim->name)) == NULL) {
            char newname[17];

            snprintf(newname, sizeof newname, "%s.M", dim->name);
            if ((bnd = load_as_dimbound(newname)) == NULL)
                goto finish;
        }
        bounds = bnd->bnd + astr - 1;
    }

    if (zslice) {
        if (adef->must_have_bounds)
            logging(LOG_WARN, "Do not slice %s.", adef->id);

        bounds = NULL;  /* if sliced, bounds are spoiled. */
        values = slice_gtdim(&dimlen, dim, astr, aend, zslice);
    } else {
        values = dim->values + astr - 1;
        dimlen = aend - astr + 1;
    }

    axisid = adef->index_only != 'n'
        ? get_axis_by_index(adef->id, dimlen)
        : get_axis_by_values(adef->id,
                             dim->unit,
                             values,
                             bounds,
                             dimlen);

finish:
    GT3_freeDimBound(bnd);
    if (zslice)
        free(values);

    return axisid;
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

    /*
     * special case: return 2 axis-IDs (for optical thickness and pressure).
     */
    if (strcmp(aitm, "tauplev") == 0) {
        assert(!"not yet implemented");
    }

    if (strncmp(aitm, "CSIG", 4) == 0) {
        adef = lookup_axisdef("standard_hybrid_sigma");
        if (!adef) {
            logging(LOG_ERR, "standard_bybrid_sigma not defined in MIP-table");
            goto finish;
        }
    } else if (strncmp(aitm, "HETA", 4) == 0) {
        adef = lookup_axisdef("standard_hybrid_sigma");
        if (!adef) {
            logging(LOG_ERR, "standard_hybrid_sigma not defined in MIP-table");
            goto finish;
        }
    } else {
        adef = lookup_axisdef_in_vardef(dim->title, vdef);
        if (!adef) {
            logging(LOG_ERR, "No such axis: %s", dim->title);
            goto finish;
        }
    }
    if ((axisid = get_axisid(dim, astr, aend, adef, zslice)) < 0)
        goto finish;

    ids[0] = axisid;
    *nids = 1;
    rval = 0;

finish:
    GT3_freeDim(dim);
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

    {
        GT3_Dim *plev;
        double *values;
        struct sequence *zslice;
        int num = 0;

        plev = GT3_getDim("ECMANLP23");
        assert(plev);

        zslice = initSeq("1:3,5:18", 1, 23);
        assert(zslice);

        values = slice_gtdim(&num, plev, 1, 23, zslice);
        assert(num == 17);
        assert(values[0] == 1000.);
        assert(values[1] == 925.);
        assert(values[2] == 850.);
        assert(values[3] == 700.);
        assert(values[16] == 10.);
        free(values);


        freeSeq(zslice);
        zslice = initSeq("23:19:-1", 1, 23);
        assert(zslice);

        values = slice_gtdim(&num, plev, 1, 23, zslice);
        assert(num == 5);
        assert(values[0] == 1.);
        assert(values[1] == 2.);
        assert(values[2] == 3.);
        assert(values[3] == 5.);
        assert(values[4] == 7.);
        free(values);

        freeSeq(zslice);
        zslice = initSeq("-5:-1", 1, 23);
        assert(zslice);

        values = slice_gtdim(&num, plev, 1, 23, zslice);
        assert(num == 5);
        assert(values[0] == 7.);
        assert(values[1] == 5.);
        assert(values[2] == 3.);
        assert(values[3] == 2.);
        assert(values[4] == 1.);
        free(values);
    }

    {
        double lon_bnds[] = {0., 180., 360.};
        double lon[] = { 90., 270. };
        double lat_bnds[] = {-90., 0., 90.};
        double lat[] = { -45., 45 };
        double sigma[] = {0.9, 0.5, .1};
        double sigma_bnds[] = {1., .75, .25, 0.};
        double ps[] = { 900., 950., 1000., 1050. };

        int axisid[4];
        int sigma_id, ptop_id, ps_id;
        int axisid2[3];
        double ptop = 0.;

        axisid[0] = get_axis_by_values("longitude", "degrees_east",
                                       lon, lon_bnds, 2);

        axisid[1] = get_axis_by_values("latitude", "degrees_north",
                                       lat, lat_bnds, 2);

        axisid[2] = get_axis_by_values("standard_sigma", "",
                                       sigma, sigma_bnds, 3);

        cmor_axis(axisid + 3, "time", "days since 1950-1-1", 1,
                  NULL, 'd', NULL, 0, NULL);

        cmor_zfactor(&ptop_id, axisid[2], "ptop", "Pa",
                     0, NULL, 'd', &ptop, NULL);

        cmor_zfactor(&sigma_id, axisid[2], "sigma", "1",
                     1, axisid + 2, 'd', sigma, sigma_bnds);

        axisid2[0] = axisid[0];
        axisid2[1] = axisid[1];
        axisid2[2] = axisid[3];
        cmor_zfactor(&ps_id, axisid[2], "ps", "hPa",
                     3, axisid2, 'd', ps, NULL);
    }

    {
        cmor_axis_def_t *adef;
        GT3_Dim *dim;
        int axisid;

        dim = GT3_getDim("GGLA64");
        assert(dim);
        adef = lookup_axisdef("latitude");
        assert(adef);

        axisid = get_axisid(dim, 1, 64, adef, NULL);
        assert(axisid >= 0);
    }

    {
        cmor_axis_def_t *adef;
        GT3_Dim *dim;
        int axisid;

        dim = GT3_getDim("CSIG20");
        assert(dim);
        adef = lookup_axisdef("standard_hybrid_sigma");
        assert(adef);
        assert(strcmp(adef->id, "standard_hybrid_sigma") == 0);

        axisid = get_axisid(dim, 1, 20, adef, NULL);
        assert(axisid >= 0);
        printf("(%s)\n", cmor_axes[axisid].id);
    }


    printf("test_axis(): DONE\n");
    return 0;
}
#endif /* TEST_MAIN2 */
