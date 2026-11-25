/*
 * axis.c
 */
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gtool3.h"
#include "logging.h"
#include "cmor.h"
#include "cmor_supp.h"
#include "internal.h"
#include "myutils.h"

#ifndef PATH_MAX
#  define PATH_MAX 1024
#endif

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
                  (double *)bnds, bnd_ndim, NULL) != 0) {

        logging(LOG_ERR, "cmor_axis() failed.");
        return -1;
    }
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
                  NULL, 0, NULL) != 0) {

        logging(LOG_ERR, "cmor_axis() failed.");
        return -1;
    }
    free(idx);
    return newid;
#else
    return get_axis_by_values(name, NULL, NULL, NULL, len);
#endif
}


static void
get_char_values(char *values, int step, int nelems,
                const double *idx, FILE *fp)
{
    int lineno, cnt;

    for (lineno = 0, cnt = 0; cnt < nelems; lineno++) {
        read_logicline(values, step, fp);

        if (idx == NULL || lineno == (int)idx[cnt]) {
            values += step;
            cnt++;
        }
    }
}


/*
 * get an axis whose type is character.
 *
 * read strings from *.txt, such as GTAXLOC.BASIN03.txt
 */
static int
get_axis_of_chars(const char *name, const char *aitm,
                  const double *idx, int len)
{
    const size_t MAXLEN_CAXIS = 128;
    GT3_File *gf = NULL;
    FILE *fp = NULL;
    int newid = -1;
    char *values = NULL;
    char path[PATH_MAX + 1];

    if ((gf = GT3_openAxisFile(aitm)) == NULL) {
        GT3_printErrorMessages(stderr);
        return -1;
    }
    snprintf(path, PATH_MAX, "%s.txt", gf->path);
    GT3_close(gf);

    if ((fp = fopen(path, "r")) == NULL) {
        logging(LOG_SYSERR, path);
        goto finish;
    }

    if ((values = malloc(len * MAXLEN_CAXIS)) == NULL) {
        logging(LOG_SYSERR, NULL);
        goto finish;
    }

    get_char_values(values, MAXLEN_CAXIS, len, idx, fp);
    if (cmor_axis(&newid, (char *)name, "1", len,
                  values, 'c',
                  NULL, MAXLEN_CAXIS, NULL) != 0) {
        logging(LOG_ERR, "cmor_axis() failed.");
        return -1;
    }
finish:
    fclose(fp);
    free(values);
    return newid;
}


/*
 * Return 1 if match and 0 if not match.
 *
 * '#' in `pattern` is a wild card for digits.
 */
int
match_axisname(const char *name, const char *pattern)
{
    for (; *pattern != '\0'; name++, pattern++)
        if (!((*pattern == '#' && isdigit(*name)) || *pattern == *name))
            return 0;

    return 1;
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


GT3_DimBound *
get_dimbound(const char *name)
{
    GT3_DimBound *bnd;
    char newname[17];

    if ((bnd = GT3_getDimBound(name)) == NULL) {
        snprintf(newname, sizeof newname, "%s.M", name);
        bnd = load_as_dimbound(newname);
    }
    return bnd;
}


static int
get_axisid(const GT3_Dim *dim,
           int astr, int aend,
           const cmor_axis_def_t *adef,
           struct sequence *zslice)
{
    double *values = NULL;
    double *bounds = NULL;
    int dimlen = 0;
    GT3_DimBound *bnd = NULL;
    int axisid = -1;

    if (adef->must_have_bounds) {
        if ((bnd = get_dimbound(dim->name)) == NULL)
            goto finish;

        bounds = bnd->bnd + astr - 1;
    }

    if (zslice) {
        if (bounds) {
            int first, last, step;

            checkSeq(zslice, &first, &last, &step);
            if (step != 1) {
                bounds = NULL;
                logging(LOG_WARN, "invalidate bounds for %s.", adef->id);
            } else
                bounds += first - astr;
        }
        values = slice_gtdim(&dimlen, dim, astr, aend, zslice);
    } else {
        values = dim->values + astr - 1;
        dimlen = aend - astr + 1;

        if (adef->index_only == 'n'
            && dimlen > dim->len - dim->cyclic + 1 - astr) {
            logging(LOG_ERR, "%s: Too short axis.", dim->name);
            goto finish;
        }
    }

    if (adef->type == 'c')
        axisid = get_axis_of_chars(adef->id,
                                   dim->name,
                                   zslice ? values : NULL,
                                   dimlen);
    else
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
 *    if (get_axis_ids(ids, &nids, "GLON128", 1, 128, slice, vdef, head) < 0) {
 *    }
 */
int
get_axis_ids(int *ids, int *nids, generic_axis *glev,
             const char *aitm, int astr, int aend,
             struct sequence *slice,
             const cmor_var_def_t *vdef,
             const GT3_HEADER *head)
{
    GT3_Dim *dim;
    cmor_axis_def_t *adef = NULL;
    int rval = -1;
    int axisid;

    *nids = 0;
    if (aend - astr <= 0 && dummy_dimname(aitm)) {
        logging(LOG_INFO, "skip empty dim (%s).", aitm);
        return 0;
    }

    /*
     * MEMO1000:
     * To support coordinates which have four or more dimensions,
     * the MEMO? (1..5) fields are used instead of AITM3
     * if the value of AITM3 is "MEMO1000".
     *
     * Values in MEMO? are assumed as "axisname/astr:aend".
     */
    if (strcmp(aitm, "MEMO1000") == 0) {
        char key[17], value[17];
        char axisname[17], *p;
        int range[2];
        int i, dummy;

        for (i = 0; i < 5; i++) {
            sprintf(key, "MEMO%d", i + 1);
            GT3_copyHeaderItem(value, sizeof value, head, key);
            if (value[0] == '\0')
                break;

            p = split2(axisname, sizeof axisname, value, "/");
            if (get_ints(range, 2, p, ':') != 2) {
                logging(LOG_ERR, "Invalid MEMO1000: %s (%s)", key, value);
                return -1;
            }

            if (get_axis_ids(&axisid, &dummy, NULL,
                             axisname, range[0], range[1],
                             NULL, vdef, head) < 0)
                return -1;

            assert(dummy == 1);
            ids[i] = axisid;
        }

        *nids = i;
        return 0;
    }

    /* For compatibility. */
    if (strcmp(aitm, "ISCCPTP49") == 0) {
        int tau7, plev7, num;

        if (get_axis_ids(&tau7, &num, NULL, "ISCCPTAU7",
                         1, 7, NULL, vdef, head) < 0
            || get_axis_ids(&plev7, &num, NULL, "ISCCPPL7",
                            1, 7, NULL, vdef, head) < 0)
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

    if (dim->title) {
        adef = lookup_axisdef_in_vardef(dim->title, vdef);
    }
    if (adef)
        logging(LOG_INFO, "found '%s' in %s(%s)", adef->id, aitm, dim->title);

    if (!adef && has_modellevel_dim(vdef)) {
        /*
         * model level axis
         * (we need to set zfactor later except for depth_coord).
         */
        struct { const char *key, *value; } tab[] = {
            {"CSIG##.M",  "standard_sigma_half"},
            {"CSIG###.M", "standard_sigma_half"},
            {"HETA##.M",  "standard_hybrid_sigma_half"},
            {"HETA###.M", "standard_hybrid_sigma_half"},
            {"CSIG",      "standard_sigma"},
            {"HETA",      "standard_hybrid_sigma"},
            {"CETA",      "standard_hybrid_sigma"},
            {"OCDEPT",    "ocean_sigma_z"},
            {"OCDEPM",    "ocean_sigma_z"},
            {"OCLEV",     "depth_coord"}
        };
        int n;

        assert(glev != NULL);
        for (n = 0; n < sizeof tab / sizeof tab[0]; n++)
            if (match_axisname(aitm, tab[n].key)) {
                adef = lookup_axisdef(tab[n].value);
                if (!adef)
                    logging(LOG_WARN, "%s: No such aixs in MIP table.",
                            tab[n].value);

                /*
                 * Set genric level property.
                 */
                strcpy(glev->id, tab[n].value);
                strcpy(glev->aitm, aitm);
                glev->astr = astr;
                glev->aend = aend;
                break;
            }
    }
    if (!adef) {
        logging(LOG_ERR,
                "cannot find axis-def."
                " check TITLE field in GTAXLOC.%s.", aitm);
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
