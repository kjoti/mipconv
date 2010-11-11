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
#include "myutils.h"

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
        || (bnd->bnd = malloc(sizeof(double) * dim->len)) == NULL
        || (bnd->name = strdup(dim->name)) == NULL) {
        logging(LOG_SYSERR, NULL);
        return NULL;
    }

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
 * unit: unit of values (It may differ from MIP-table)
 */
static int
get_axis_by_values(const char *name,
                   const char *unit,
                   const double *values,
                   const double *bnds,
                   int len)
{
    int bnd_ndim = bnds ? 1 : 0;
    int newid;

    if (cmor_axis(&newid, (char *)name, (char *)unit, len,
                  (double *)values, 'd',
                  (double *)bnds, bnd_ndim, NULL) < 0) {

        logging(LOG_ERR, "cmor_axis() failed");
        return -1;
    }
    assert(newid >= 0);
    return newid;
}


/*
 * In the case of 'index_only: yes'.
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
    return get_axis_by_values(name, NULL, NULL, NULL, len);
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
 *    if (get_axis_ids(ids, &nids, "GLON128", 1, 128, slice, vdef) < 0) {
 *    }
 */
int
get_axis_ids(int *ids, int *nids,
             const char *aitm, int astr, int aend,
             struct sequence *slice,
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

    if (strcmp(aitm, "ISCCPTP49") == 0) {
        int tau7, plev7, num;

        if (get_axis_ids(&tau7, &num, "ISCCPTAU7",
                         1, 7, NULL, vdef) < 0
            || get_axis_ids(&plev7, &num, "ISCCPPL7",
                            1, 7, NULL, vdef) < 0)
            return -1;

        ids[0] = tau7;
        ids[1] = plev7;
        *nids = 2;
        return 0;
    }

    if ((dim = GT3_getDim(aitm)) == NULL) {
        GT3_printErrorMessages(stderr);
        return -1;
    }

    adef = lookup_axisdef_in_vardef(dim->title, vdef);
    if (adef)
        logging(LOG_INFO, "found \"%s\"", dim->title);

    if (!adef && has_modellevel_dim(vdef)) {
        /*
         * model level axis (we need to set zfactor later).
         */
        struct { const char *key; const char *value; } tab[] = {
            { "CSIG", "standard_hybrid_sigma" },
            { "HETA", "standard_hybrid_sigma" },
            { "OCDEP", "ocean_sigma_z" }
        };
        int n;

        for (n = 0; n < sizeof tab / sizeof tab[0]; n++)
            if (startswith(aitm, tab[n].key)) {
                adef = lookup_axisdef(tab[n].value);
                if (!adef)
                    logging(LOG_WARN, "%s: No such aixs in MIP table",
                            tab[n].value);

                free(dim->unit);
                dim->unit = strdup("1");
                break;
            }
    }
    if (!adef) {
        logging(LOG_ERR, "No axis for %s", aitm);
        goto finish;
    }
    if ((axisid = get_axisid(dim, astr, aend, adef, slice)) < 0)
        goto finish;

    ids[0] = axisid;
    *nids = 1;
    rval = 0;

finish:
    GT3_freeDim(dim);
    return rval;
}


#ifdef TEST_MAIN2
void
test_sigma(const char *sigma_name)
{
    cmor_axis_def_t *adef;
    GT3_Dim *dim;
    int axisid;
    cmor_axis_t *axis;

    dim = GT3_getDim("CSIG20");
    assert(dim);
    adef = lookup_axisdef(sigma_name);
    assert(adef);

    axisid = get_axisid(dim, 1, 20, adef, NULL);
    assert(axisid >= 0);
    axis = cmor_axes + axisid;

    logging(LOG_INFO, "axisdef id: %s", adef->id);
    logging(LOG_INFO, "axis    id: %s", axis->id);
    logging(LOG_INFO, "hybrid_in : %d", axis->hybrid_in);
    assert(axis->axis == 'Z');

    /*
     * XXX
     * No "convert_to" attribute results in hybrid_in == hybrid_out.
     */
    if (axis->hybrid_in != axis->hybrid_out) {
        logging(LOG_INFO, "convert_to: %s", adef->convert_to);
        assert(adef->convert_to[0] != '\0');
    }
    GT3_freeDim(dim);
}

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
        assert(axis->length == 4);
        assert(axis->bounds[0] == -90.);
        assert(axis->bounds[1] == -45.);
        assert(axis->bounds[2] == -45.);
        assert(axis->bounds[3] == 0.);
        assert(axis->bounds[7] == 90.);
        assert(axis->values[0] == -67.5);
        assert(axis->values[1] == -22.5);
        /* assert(axis->revert == -1); */
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
        GT3_freeDim(plev);
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

    test_sigma("standard_sigma");
    test_sigma("standard_hybrid_sigma");

    if (0) {
        int axisid;
        cmor_axis_t *axis;


        axisid = get_axis_by_index("alevhalf", 57);
        assert(axisid >= 0);

        axis = cmor_axes + axisid;
        assert(axis->length == 57);
        /* assert(axis->store_in_netcdf == 0); */
    }

    printf("test_axis(): DONE\n");
    return 0;
}
#endif /* TEST_MAIN2 */
