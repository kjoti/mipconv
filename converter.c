/*
 * converter.c
 */
#include <assert.h>
#include <stdio.h>

#include "gtool3.h"
#include "logging.h"
#include "var.h"

#include "cmor.h"
#include "cmor_supp.h"
#include "internal.h"


/*
 * definition of axis slicing.
 *
 * XXX: axis_slice[0] and axis_slice[1] are not used
 * in current implementation.
 */
static struct sequence  *axis_slice[] = { NULL, NULL, NULL };


int
set_axis_slice(int idx, const char *spec)
{
    struct sequence  *seq;

    assert(idx >= 0 && idx < 3);
    freeSeq(axis_slice[idx]);
    axis_slice[idx] = NULL;

    if ((seq = initSeq(spec, 0, 0x7ffffff)) == NULL)
        return -1;

    axis_slice[idx] = seq;
    return 0;
}


/*
 * Some files has invalid units, which UDUNITS2 cannot recognize.
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
    int varid;
    int status;
    int i, j, n, ndims;
    int axis[CMOR_MAX_DIMENSIONS], ids[CMOR_MAX_DIMENSIONS], nids;
    int astr, aend;
    float miss;
    char title[33], unit[17], aitm[17];
    cmor_axis_def_t *axisdef;
    cmor_axis_def_t *timedef = NULL;
    int ntimedim = 0;           /* 0 or 1 */
    char positive = '\0';       /* 'u', 'd', '\0' */

    /*
     * check required dimensions of the variable.
     */
    for (i = 0, ndims = vdef->ndims; i < vdef->ndims; i++) {
        axisdef = get_axisdef_in_vardef(vdef, i);
        if (axisdef == NULL)
            continue;

        /* Singleton dimension can be ignored here. */
        if (is_singleton(axisdef)) {
            ndims--;
            continue;
        }
        if (axisdef->axis == 'T') {
            assert(ntimedim == 0);
            ntimedim = 1;
            timedef = axisdef;
            ndims--;
            continue;
        }
    }

    /*
     * setup axes (except for time-axis).
     */
    for (i = 0, n = 0; i < 3 && n < ndims; i++) {
        get_axis_prof(aitm, &astr, &aend, head, i);

        if (axis_slice[i])
            reinitSeq(axis_slice[i], astr, aend);

        if (get_axis_ids(ids, &nids, aitm, astr, aend,
                         axis_slice[i], vdef) < 0) {
            logging(LOG_ERR, "%s: failed to get axis-id", aitm);
            return -1;
        }
        for (j = 0; j < nids; j++, n++) {
            logging(LOG_INFO, "axisid = %d for %s", ids[j], aitm);
            if (n < CMOR_MAX_DIMENSIONS)
                axis[n] = ids[j];
        }
    }
    if (n != ndims) {
        logging(LOG_ERR, "Axes mismatch between input data and MIP-table");
        return -1;
    }

    if (ntimedim) {
        axis[ndims] = get_timeaxis(timedef);
        logging(LOG_INFO, "axisid = %d for time", axis[ndims]);
    }

    /* e.g., lon, lat, lev, time => time, lev, lat, lon. */
    reverse_iarray(axis, ndims + ntimedim);

    GT3_copyHeaderItem(title, sizeof title, head, "TITLE");
    GT3_copyHeaderItem(unit, sizeof unit, head, "UNIT");
    translate_unit(unit, sizeof unit);
    miss = (float)(vbuf->miss);
    status = cmor_variable(&varid, (char *)vdef->id, unit,
                           ndims + ntimedim, axis,
                           'f', &miss,
                           NULL, &positive, title,
                           NULL, NULL);
    if (status != 0) {
        logging(LOG_ERR, "cmor_variable() failed.");
        return -1;
    }
    logging(LOG_INFO, "varid = %d for %s", varid, vdef->id);
    return varid;
}


/*
 * Return
 *   0: independent of time.
 *   1: depend on time (snapshot).
 *   2: depend on time (time-mean).
 */
static int
check_timedepend(const cmor_var_def_t *vdef)
{
    int timedepend = 0;
    cmor_axis_def_t *axisdef;
    int i;

    for (i = 0; i < vdef->ndims; i++) {
        axisdef = get_axisdef_in_vardef(vdef, i);

        if (axisdef && axisdef->axis == 'T') {
            timedepend = 1;

            if (axisdef->must_have_bounds)
                timedepend = 2;
        }
    }
    return timedepend;
}


static int
write_var(int var_id, const myvar_t *var, int *ref_varid)
{
    double *timep = NULL;
    double *tbnd = NULL;
    int ntime = 0;

    if (var->timedepend >= 1) {
        timep = (double *)(&var->time);
        ntime = 1;
    }

    if (var->timedepend == 2)
        tbnd = (double *)(var->timebnd);

    if (cmor_write(var_id, var->data, var->typecode, NULL, ntime,
                   timep, tbnd, ref_varid) != 0) {
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


static int
check_date(const GT3_HEADER *head,
           const GT3_Date *ref_date1, const GT3_Date *ref_date2)
{
    GT3_Date date1, date2;

    if (GT3_decodeHeaderDate(&date1, head, "DATE1") < 0
        || GT3_decodeHeaderDate(&date2, head, "DATE2") < 0) {

        logging(LOG_ERR, "");
        return -1;
    }

    if (GT3_cmpDate2(&date1, ref_date1) != 0
        || GT3_cmpDate2(&date2, ref_date2) != 0) {
        logging(LOG_ERR, "");
        return -1;
    }
    return 0;
}


/*
 * main routine of mipconv.
 *
 * varname: a string or NULL.
 * varcnt: 1, 2, 3, ...
 */
int
convert(const char *varname, const char *path, int varcnt)
{
    static GT3_Varbuf *vbuf = NULL;
    static int varid;
    static int first_varid;
    static cmor_var_def_t *vdef;
    static myvar_t *var = NULL;
    static GT3_Date date1;
    static GT3_Date date2;
    static int zfac_ids[8];
    static int nzfac;

    GT3_File *fp;
    GT3_HEADER head;
    int rval = -1;
    int *ref_varid;

    if ((fp = GT3_open(path)) == NULL) {
        GT3_printErrorMessages(stderr);
        return -1;
    }

    if (varname) {
        int shape[3];

        if ((vdef = lookup_vardef(varname)) == NULL) {
            logging(LOG_ERR, "%s: No such variable in MIP table", varname);
            goto finish;
        }

        GT3_freeVarbuf(vbuf);
        vbuf = NULL;
        free_var(var);
        var = NULL;

        if (GT3_readHeader(&head, fp) < 0
            || (vbuf = GT3_getVarbuf(fp)) == NULL
            || (var = new_var()) == NULL) {
            GT3_printErrorMessages(stderr);
            goto finish;
        }

        if (varcnt == 1) {
            if ((varid = get_varid(vdef, vbuf, &head)) < 0)
                goto finish;

            if ((nzfac = setup_zfactors(zfac_ids, varid, &head)) < 0)
                goto finish;

            first_varid = varid;
        } else {
            /*
             * XXX for zfactor, such as ps.
             */
            if (varcnt > nzfac + 1) {
                logging(LOG_ERR, "No more zfactor");
                goto finish;
            }
            varid = zfac_ids[varcnt - 2];
        }

        shape[0] = vbuf->dimlen[0];
        shape[1] = vbuf->dimlen[1];
        shape[2] = vbuf->dimlen[2];
        if (axis_slice[2]) {
            rewindSeq(axis_slice[2]);
            shape[2] = countSeq(axis_slice[2]);
        }
        if (resize_var(var, shape, 3) < 0)
            goto finish;

        /*
         * initialize DATE1 and DATE2 from GTOOL3 header.
         */
        var->timedepend = check_timedepend(vdef);
        if (var->timedepend > 0) {
            if (   GT3_decodeHeaderDate(&date1, &head, "DATE1") < 0
                || GT3_decodeHeaderDate(&date2, &head, "DATE2") < 0) {
                GT3_printErrorMessages(stderr);
                logging(LOG_ERR, "missing DATE1 and/or DATE2");
                goto finish;
            }

            if (set_timeinterval(get_frequency(vdef)) < 0) {
                logging(LOG_ERR, "missing frequency:");
                goto finish;
            }
        }
    } else {
        assert(vbuf != NULL);
        if (GT3_reattachVarbuf(vbuf, fp) < 0)
            goto finish;
    }

    ref_varid = varcnt == 1 ? NULL : &first_varid;

    while (!GT3_eof(fp)) {
        if (GT3_readHeader(&head, fp) < 0) {
            GT3_printErrorMessages(stderr);
            goto finish;
        }

        /*
         * check & set DATE/TIME.
         */
        if (var->timedepend > 0) {
            if (check_date(&head, &date1, &date2) < 0) {
                logging(LOG_ERR, "");
                goto finish;
            }
            var->timebnd[0] = get_time(&date1);
            var->timebnd[1] = get_time(&date2);
            var->time = .5 * (var->timebnd[0] + var->timebnd[1]);
        }

        if (read_var(var, vbuf, axis_slice[2]) < 0
            || tweak_var(var, vdef) < 0
            || write_var(varid, var, ref_varid) < 0) {
            goto finish;
        }

        if (GT3_next(fp) < 0) {
            GT3_printErrorMessages(stderr);
            goto finish;
        }

        if (var->timedepend > 0) {
            step_time(&date1);
            step_time(&date2);
        }
    }
    rval = 0;

finish:
    GT3_close(fp);
    return rval;
}


#ifdef TEST_MAIN2
int
test_converter(void)
{
    cmor_var_def_t *vdef;
    int tdep;

    vdef = lookup_vardef("tas");
    tdep = check_timedepend(vdef);
    assert(tdep == 2);

    printf("test_converter(): DONE\n");
    return 0;
}
#endif /* TEST_MAIN2 */
