#include <assert.h>
#include <stdio.h>

#include "gtool3.h"
#include "logging.h"
#include "var.h"

#include "cmor.h"
#include "cmor_supp.h"
#include "utils.h"
#include "internal.h"

#ifndef CMOR_MAX_DIMENSIONS
#  define CMOR_MAX_DIMENSIONS 7
#endif

static int
get_axis_prof(char *name, int *istr, int *iend,
              const GT3_HEADER *head, int idx)
{
    static const char *aitm[] = { "AITM1", "AITM2", "AITM3" };
    static const char *astr[] = { "ASTR1", "ASTR2", "ASTR3" };
    static const char *aend[] = { "AEND1", "AEND2", "AEND3" };

    assert(idx >= 0 && idx < 3);

    if (GT3_copyHeaderItem(name, 17, head, aitm[idx]) == NULL
        || GT3_decodeHeaderInt(istr, head, astr[idx]) < 0
        || GT3_decodeHeaderInt(iend, head, aend[idx]) < 0)
        return -1;

    return 0;
}


static int
get_timeaxis(void)
{
    int axis;

    if (cmor_axis(&axis, "time", "day since 0-1-1", 1,
                  NULL, 'd', NULL, 0, NULL) != 0) {
        logging(LOG_ERR, "cmor_axis() failed.");
        return -1;
    }
    return axis;
}


/*
 *  Some files has invalid units, which UDUNITS2 cannot recognize.
 */
static void
translate_unit(char *unit, size_t len)
{
}


static int
get_varid(const cmor_var_def_t *vdef,
          const GT3_Varbuf *vbuf,
          const GT3_HEADER *head)
{
    int timedepend = 0;
    int varid;
    int status;
    int i, j, n, ndims;
    int axis[CMOR_MAX_DIMENSIONS], ids[CMOR_MAX_DIMENSIONS], nids;
    int astr, aend;
    float miss;
    char title[33], unit[17], aitm[17];
    cmor_axis_def_t *axisdef;


    /*
     * check required dimension of the variable.
     */
    for (i = 0, ndims = vdef->ndims; i < vdef->ndims; i++) {
        axisdef = get_axisdef_in_vardef(vdef, i);

        /* Singleton dimension can be ignored here. */
        if (is_singleton(axisdef)) {
            ndims--;
            continue;
        }
        if (axisdef->axis == 'T') {
            assert(timedepend == 0);
            timedepend = 1;
            ndims--;
            continue;
        }
    }

    /*
     * setup axes (except for time-axis).
     */
    for (i = 0, n = 0; i < 3 && n < ndims; i++) {
        get_axis_prof(aitm, &astr, &aend, head, i);

        if (get_axis_ids(ids, &nids, aitm, astr, aend, NULL, vdef) < 0) {
            logging(LOG_ERR, "%s: failed to get axis-id", aitm);
            return -1;
        }
        for (j = 0; j < nids; j++, n++) {
            logging(LOG_INFO, "axisid = %d, for %s", ids[j], aitm);
            if (n < CMOR_MAX_DIMENSIONS)
                axis[n] = ids[j];
        }
    }
    if (n != ndims) {
        logging(LOG_ERR, "Axes mismatch between input data and MIP-table");
        return -1;
    }

    if (timedepend)
        axis[ndims] = get_timeaxis();

    /* e.g., lon, lat, lev, time => time, lev, lat, lon. */
    reverse_iarray(axis, ndims + timedepend);

    GT3_copyHeaderItem(title, sizeof title, head, "TITLE");
    GT3_copyHeaderItem(unit, sizeof unit, head, "UNIT");
    translate_unit(unit, sizeof unit);
    miss = (float)(vbuf->miss);
    status = cmor_variable(&varid, (char *)vdef->id, unit,
                           ndims + timedepend, axis,
                           'f', &miss,
                           NULL, NULL, title,
                           NULL, NULL);
    if (status != 0) {
        logging(LOG_ERR, "cmor_variable() failed.");
        return -1;
    }
    return varid;
}


static int
write_var(int var_id, const myvar_t *var)
{
    double time = var->time;

    if (cmor_write(var_id, var->data, var->typecode, NULL, 1,
                   &time, (double *)var->timebnd, NULL) != 0) {

        logging(LOG_ERR, "cmor_write() failed");
        return -1;
    }
    return 0;
}



static int
tweak_var(myvar_t *var, cmor_var_def_t *vdef)
{
    return 0;
}


int
convert(const char *varname, const char *path, int cnt)
{
    static GT3_Varbuf *vbuf = NULL;
    static int varid;
    GT3_File *fp;
    GT3_HEADER head;
    cmor_var_def_t *vdef;
    static myvar_t *var = NULL;


    if ((fp = GT3_open(path)) == NULL) {
        GT3_printErrorMessages(stderr);
        return -1;
    }

    if (cnt == 0) {
        if ((vdef = lookup_vardef(varname)) == NULL) {
            logging(LOG_ERR, "%s: No such variable in MIP table", varname);
            GT3_close(fp);
            return -1;
        }

        GT3_freeVarbuf(vbuf);
        vbuf = NULL;
        free_var(var);
        var = NULL;

        if (GT3_readHeader(&head, fp) < 0
            || (vbuf = GT3_getVarbuf(fp)) == NULL) {
            GT3_printErrorMessages(stderr);
            GT3_close(fp);
            return -1;
        }

        /* modify HEADER for z-slicing. */
        /* FIXME */

        if ((varid = get_varid(vdef, vbuf, &head)) < 0
            || (var = new_var()) == NULL
            || resize_var(var, vbuf->dimlen, 3) < 0) {
            return -1;
        }
    } else {
        assert(vbuf != NULL);
        if (GT3_reattachVarbuf(vbuf, fp) < 0)
            return -1;
    }

    while (!GT3_eof(fp)) {
        if (read_var(var, vbuf) < 0
            || tweak_var(var, vdef) < 0
            || write_var(varid, var) < 0) {
            GT3_close(fp);
            return -1;
        }
        if (GT3_next(fp) < 0) {
            GT3_printErrorMessages(stderr);
            GT3_close(fp);
            return -1;
        }
    }
    GT3_close(fp);
    return 0;
}
