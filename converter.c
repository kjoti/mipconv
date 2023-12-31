/*
 * converter.c -- the heart of mipconv.
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gtool3.h"
#include "logging.h"
#include "var.h"

#include "cmor.h"
#include "cmor_supp.h"
#include "internal.h"
#include "myutils.h"
#include "fileiter.h"
#include "site.h"

/*
 * positive: 'u', 'd',  or '\0'.
 */
static char positive = '\0';

/*
 * Grid mappings.
 */
enum {
    LATITUDE_LONGITUDE = 0, /* 0: This is dummy. */
    ROTATED_POLE,
    BIPOLAR,
    TRIPOLAR
};
static int grid_mapping = LATITUDE_LONGITUDE;

/*
 * Site locations.
 */
static site_locations *sites = NULL;
static float *site_databuf = NULL; /* XXX: must be of float */
static size_t site_databuf_capacity = 0;

/*
 * use GT3_open() instead of GT3_openHistFile() if safe_open_mode.
 */
static int safe_open_mode = 0;

/*
 * time dependency.
 */
enum {
    TIME_INDEP,
    TIME_POINT,
    TIME_MEAN,
    TIME_CLIM
};


/*
 * axis slicing.
 *
 * XXX: axis_slice[0] and axis_slice[1] are not used
 * in current implementation.
 */
static struct sequence  *axis_slice[] = { NULL, NULL, NULL };

/*
 * time slicing.
 */
static struct sequence *time_seq = NULL;

/*
 * deflate.
 */
static int shuffle = 1;
static int deflate = 1;
static int deflate_level = 6;   /* from 0 to 9 */


int
set_deflate_level(int level)
{
    if (level < 0 || level > 9)
        return -1;

    deflate = level > 0 ? 1 : 0;
    deflate_level = level;
    return 0;
}


int
set_shuffle(int s)
{
    if (s < 0 || s > 1)
        return -1;

    shuffle = s;
    return 0;
}


void
set_safe_open(void)
{
    safe_open_mode = 1;
}


int
set_axis_slice(int idx, const char *spec)
{
    struct sequence  *seq;

    assert(idx >= 0 && idx < 3);
    freeSeq(axis_slice[idx]);
    axis_slice[idx] = NULL;

    if ((seq = initSeq(spec, 1, 0x7ffffff)) == NULL) {
        logging(LOG_SYSERR, NULL);
        return -1;
    }
    axis_slice[idx] = seq;
    return 0;
}


void
unset_axis_slice(void)
{
    axis_slice[0] = NULL;
    axis_slice[1] = NULL;
    axis_slice[2] = NULL;
}


void
unset_positive(void)
{
    positive = '\0';
}


int
set_positive(const char *str)
{
    if (str[0] == 'u' || str[0] == 'd') {
        positive = str[0];
        logging(LOG_INFO, "set positive: %c", str[0]);
        return 0;
    }
    logging(LOG_ERR, "%s: invalid parameter for positive.", str);
    return -1;
}


int
set_time_slice(const char *str)
{
    freeSeq(time_seq);
    time_seq = NULL;

    if (strcmp(str, ":") == 0)
        return 0;

    time_seq = initSeq(str, 1, 0x7fffffff);
    return time_seq != NULL ? 0 : -1;
}


/*
 * expression for eval_calc(calculator.c).
 */
static char *calc_expression = NULL;


void
unset_calcexpr(void)
{
    free(calc_expression);
    calc_expression = NULL;
}


int
set_calcexpr(const char *str)
{
    unset_calcexpr();
    calc_expression = strdup(str);
    if (calc_expression == NULL) {
        logging(LOG_SYSERR, NULL);
        return -1;
    }
    return 0;
}


int
set_grid_mapping(const char *name)
{
    struct { const char *key; int value; } tab[] = {
        { "rotated_pole", ROTATED_POLE },
        { "bipolar", BIPOLAR },
        { "tripolar", TRIPOLAR }
    };
    int i;

    for (i = 0; i < sizeof tab / sizeof tab[0]; i++)
        if (strcmp(name, tab[i].key) == 0) {
            grid_mapping = tab[i].value;
            return 0;
        }
    return -1;
}


int
set_site_locations(const char *path)
{
    sites = load_site_locations(path);

    return sites ? 0 : -1;
}


int
get_dim_prop(gtool3_dim_prop *dim,
             const GT3_HEADER *head, int idx)
{
    static const char *aitm[] = { "AITM1", "AITM2", "AITM3" };
    static const char *astr[] = { "ASTR1", "ASTR2", "ASTR3" };
    static const char *aend[] = { "AEND1", "AEND2", "AEND3" };

    assert(idx >= 0 && idx < 3);

    if (GT3_copyHeaderItem(&dim->aitm[0], 17, head, aitm[idx]) == NULL
        || GT3_decodeHeaderInt(&dim->astr, head, astr[idx]) < 0
        || GT3_decodeHeaderInt(&dim->aend, head, aend[idx]) < 0)
        return -1;

    return 0;
}


/*
 * setup grids and mapping parameters.
 */
static int
setup_grid_mapping(int *grid_id, const gtool3_dim_prop *dims, int mapping)
{
    GT3_Dim *x = NULL, *y = NULL;
    GT3_DimBound *x_bnds = NULL, *y_bnds = NULL;
    int id;
    int rval = -1;
    double *xx, *yy, *xx_bnds, *yy_bnds;
    double plat;
    int xlen, ylen;

    /*
     * get model native coordinates.
     */
    if ((x = GT3_getDim(dims[0].aitm)) == NULL
        || (y = GT3_getDim(dims[1].aitm)) == NULL
        || (x_bnds = get_dimbound(dims[0].aitm)) == NULL
        || (y_bnds = get_dimbound(dims[1].aitm)) == NULL) {
        GT3_printErrorMessages(stderr);
        goto finish;
    }

    xx = x->values + dims[0].astr - 1;
    yy = y->values + dims[1].astr - 1;
    xlen = dims[0].aend - dims[0].astr + 1;
    ylen = dims[1].aend - dims[1].astr + 1;
    xx_bnds = x_bnds->bnd + dims[0].astr - 1;
    yy_bnds = y_bnds->bnd + dims[1].astr - 1;

    /*
     * set mapping parameters.
     */
    switch (mapping) {
    case ROTATED_POLE:
        if (setup_rotated_pole(&id, xx, xx_bnds, xlen, yy, yy_bnds, ylen) < 0)
            goto finish;
        break;
    case BIPOLAR:
        if (setup_bipolar(&id, xx, xx_bnds, xlen, yy, yy_bnds, ylen) < 0)
            goto finish;
        break;
    case TRIPOLAR:
        plat = (strncmp(y->name, "OCLATTPV", 8) == 0)
            ? y->values[y->len - 1] - 90.
            : y_bnds->bnd[y_bnds->len_orig] - 90.;
        logging(LOG_INFO, "tripolar: joint latitude: %.16f", plat);
        if (setup_tripolar(&id, xx, xx_bnds, xlen, yy, yy_bnds, ylen,
                           plat) < 0)
            goto finish;
        break;
    default:
        assert(!"NOTREACHED");
        break;
    }
    rval = 0;
    *grid_id = id;

finish:
    GT3_freeDimBound(y_bnds);
    GT3_freeDimBound(x_bnds);
    GT3_freeDim(y);
    GT3_freeDim(x);
    return rval;
}


/*
 * setup axes for variables (main variable and zfactors).
 */
static int
setup_axes(int *axis_ids, int *num_ids,
           const cmor_var_def_t *vdef,
           const GT3_HEADER *head)
{
    int i, j, n, ndims;
    int num_axis_ids;
    cmor_axis_def_t *axisdef;
    cmor_axis_def_t *timedef = NULL;
    gtool3_dim_prop dims[3];
    int grid_pos1 = -1, grid_pos2 = -1;

    /*
     * count the number of axes except for singleton-axis and time-axis.
     * Singleton-axis is handled by CMOR2 implicitly, so we need not
     * setup it here.
     */
    for (i = 0, ndims = vdef->ndims; i < vdef->ndims; i++) {
        axisdef = get_axisdef_in_vardef(vdef, i);
        if (axisdef == NULL)
            continue;

        if (is_singleton(axisdef)) {
            ndims--;
            continue;
        }
        if (axisdef->axis == 'T') { /* time-axis */
            if (timedef) {
                logging(LOG_ERR, "%s: more than one time-axis?", vdef->id);
                return -1;
            }
            timedef = axisdef;
            ndims--;
            continue;
        }
    }

    /*
     * setup axis_ids (except for time-axis).
     */
    get_dim_prop(&dims[0], head, 0);
    get_dim_prop(&dims[1], head, 1);
    get_dim_prop(&dims[2], head, 2);
    for (i = 0, n = 0; i < 3 && n < ndims; i++) {
        gtool3_dim_prop *dp = dims + i;
        int ids[4], nids;

        if (sites && i == 0) {
            grid_pos1 = n;
            n++;
            continue;
        }
        if (sites && i == 1)
            continue;

        if (grid_mapping && (i == 0 || i == 1)) {
            /*
             * special treatment for axes used in grid mapping.
             * 'axis_id' will be set later.
             */
            logging(LOG_INFO, "mapping axis: %s", dp->aitm);
            axis_ids[n] = -1000; /* dummy */
            if (i == 0) grid_pos1 = n;
            if (i == 1) grid_pos2 = n;
            n++;
            continue;
        }

        if (axis_slice[i])
            reinitSeq(axis_slice[i], dp->astr, dp->aend);

        if (get_axis_ids(ids, &nids, dp->aitm, dp->astr, dp->aend,
                         axis_slice[i], vdef, head) < 0) {
            logging(LOG_ERR, "%s: failed to get axis-id.", dp->aitm);
            return -1;
        }
        for (j = 0; j < nids; j++, n++) {
            logging(LOG_INFO, "axisid = %d for %s", ids[j], dp->aitm);
            if (n < CMOR_MAX_DIMENSIONS)
                axis_ids[n] = ids[j];
        }
    }
    num_axis_ids = n;
    if (num_axis_ids != ndims) {
        logging(LOG_ERR, "Axes mismatch between input data and MIP-table.");
        return -1;
    }

    /*
     * setup grid mapping if needed.
     */
    if (grid_mapping) {
        int grid_id;

        if (grid_pos1 == -1 || grid_pos2 == -1) {
            logging(LOG_ERR, "Not ready to setup grid mapping.");
            return -1;
        }

        if (switch_to_grid_table() < 0) {
            logging(LOG_ERR, "failed to refer to a grid table.");
            return -1;
        }

        if (setup_grid_mapping(&grid_id, dims, grid_mapping) < 0)
            return -1;

        /*
         * Replace two IDs(lat/lon) by an ID of the grid mapping.
         * Removed axis_ids[grid_pos2], then decrement the number of axis_ids.
         */
        axis_ids[grid_pos1] = grid_id;
        iarray_remove(axis_ids, num_axis_ids, grid_pos2, 1);
        num_axis_ids--;

        switch_to_normal_table();
    }

    /*
     * define the grids for site locations.
     */
    if (sites) {
        int site_id, grid_id;

        if (cmor_axis(&site_id, "site", " ", sites->nlocs,
                      sites->ids, 'i', NULL, 0, NULL) != 0
            || cmor_grid(&grid_id, 1, &site_id, 'd',
                         sites->grid_lats, sites->grid_lons,
                         0, NULL, NULL) != 0) {
            logging(LOG_ERR, "failed to define the grids for site locations");
            return -1;
        }
        axis_ids[grid_pos1] = grid_id;
    }

    /*
     * setup time-axis(UNLIMITED) if needed.
     */
    if (timedef) {
        axis_ids[num_axis_ids] = get_timeaxis(timedef);
        logging(LOG_INFO, "axisid = %d for %s.",
                axis_ids[num_axis_ids], timedef->id);
        num_axis_ids++;
    }

    /* e.g., lon, lat, lev, time => time, lev, lat, lon. */
    iarray_reverse(axis_ids, num_axis_ids);

    *num_ids = num_axis_ids;
    return 0;
}


/*
 * Setup main variable to be converted.
 * Return a var_id.
 */
static int
setup_variable(const int *axis_ids, int num_axis_ids,
               const cmor_var_def_t *vdef,
               const GT3_Varbuf *vbuf,
               const GT3_HEADER *head)
{
    int varid;
    int status;
    float miss;
    char name[17];
    char unit[64]; /* XXX: The length can be exceed the limit of GTOOL3 */
    char *history, *comment;

    GT3_copyHeaderItem(name, sizeof name, head, "ITEM");
    GT3_copyHeaderItem(unit, sizeof unit, head, "UNIT");
    rewrite_unit(unit, sizeof unit);
    miss = (float)(vbuf->miss);

    history = sdb_readitem("history");
    comment = sdb_readitem("comment");
    status = cmor_variable(&varid, (char *)vdef->id, unit,
                           num_axis_ids, (int *)axis_ids,
                           'f', &miss,
                           NULL, &positive, name,
                           history, comment);

    free(history);
    free(comment);
    if (status != 0) {
        logging(LOG_ERR, "cmor_variable() failed.");
        return -1;
    }
    logging(LOG_INFO, "varid = %d for %s", varid, vdef->id);
    return varid;
}


/*
 * Check time dependency for the variable.
 *
 * Return values: TIME_INDEP, TIME_POINT, ..., TIME_CLIM
 */
static int
check_timedependency(const cmor_var_def_t *vdef)
{
    int timedepend = TIME_INDEP;
    cmor_axis_def_t *axisdef;
    int i;

    for (i = 0; i < vdef->ndims; i++) {
        axisdef = get_axisdef_in_vardef(vdef, i);

        if (axisdef && axisdef->axis == 'T') {
            timedepend = TIME_POINT;

            if (axisdef->must_have_bounds)
                timedepend = TIME_MEAN;

            if (axisdef->climatology)
                timedepend = TIME_CLIM;
        }
    }
    return timedepend;
}


/*
 * XXX CMOR2 current implementation cannot refer to nzfactors in
 * cmor_vars before cmor_write() called.
 */
static char *
required_zfactors(int var_id)
{
    cmor_var_t *var = cmor_vars + var_id;
    cmor_axis_t *axis;
    int axis_id;
    int i, j;

    for (i = 0; i < var->ndims; i++) {
        axis_id = var->axes_ids[i];

        axis = cmor_axes + axis_id;
        for (j = 0; j < axis->nattributes; j++)
            if (strcmp(axis->attributes[j], "z_factors") == 0)
                return axis->attributes_values_char[j];
    }
    return NULL;
}


static int
write_var(int var_id, const myvar_t *var, int *ref_varid)
{
    double *timep = NULL;
    double *tbnd = NULL;
    int ntimes = 0;
    float *values;

    if (var->timedepend != TIME_INDEP) {
        timep = (double *)(&var->time);
        ntimes = 1;
    }

    if (var->timedepend == TIME_MEAN || var->timedepend == TIME_CLIM)
        tbnd = (double *)(var->timebnd);

    cmor_set_deflate(var_id, shuffle, deflate, deflate_level);

    if (sites) {
        int i, k, offset;
        float *p = site_databuf;

        assert(sites->nlocs * var->dimlen[2] <= site_databuf_capacity);

        for (k = 0; k < var->dimlen[2]; k++, p += sites->nlocs) {
            offset = k * var->dimlen[0] * var->dimlen[1];

            for (i = 0; i < sites->nlocs; i++)
                p[i] = var->data[offset + sites->indexes[i]];
        }
        values = site_databuf;
    } else {
        values = var->data;
    }

    if (cmor_write(var_id, values, 'f', NULL, ntimes,
                   timep, tbnd, ref_varid) != 0) {
        logging(LOG_ERR, "cmor_write() failed.");
        return -1;
    }
    return 0;
}


/*
 * Return value:
 *   0: constant interval such as mon, da, 3hr, ...
 *  -1: otherwise, such as subhr
 */
static int
get_interval_by_freq(GT3_Duration *interval, const char *freq)
{
    struct { const char *key; int value; } dict[] = {
        { "mon",  GT3_UNIT_MON  },
        { "day",  GT3_UNIT_DAY  },
        { "hr",   GT3_UNIT_HOUR },
        { "yr",   GT3_UNIT_YEAR },
        { "hrPt", GT3_UNIT_HOUR },
        { "monC", GT3_UNIT_MON  },
    };
    char *endp;
    int i, value = 1;

    if (freq == NULL)
        return -1;

    if (isdigit(freq[0])) {
        value = strtol(freq, &endp, 10);
        freq = endp;
    }

    for (i = 0; i < sizeof dict / sizeof dict[0]; i++)
        if (strcmp(freq, dict[i].key) == 0) {
            interval->value = value;
            interval->unit = dict[i].value;
            return 0;
        }
    return -1;
}


static int
get_interval(GT3_Duration *intv, const cmor_var_def_t *vdef)
{
    return get_interval_by_freq(intv, get_frequency(vdef));
}


static int
cmp_date(const GT3_HEADER *head, const char *key, const GT3_Date *reference)
{
    GT3_Date date;

    if (GT3_decodeHeaderDate(&date, head, key) < 0) {
        GT3_printErrorMessages(stderr);
        return -1;
    }
    return GT3_cmpDate2(&date, reference);
}


/*
 * varname: a string(PCMDI name) or NULL.
 * varcnt: 1, 2, 3, ...
 */
int
convert(const char *varname, const char *path, int varcnt)
{
    static GT3_Varbuf *vbuf = NULL;
    static int varid;
    static int main_varid;
    static cmor_var_def_t *vdef;
    static myvar_t *var = NULL;
    static GT3_Date date0;
    static GT3_Date date1;
    static GT3_Date date2;
    static GT3_Duration intv;
    static int const_interval;
    static int zfac_ids[16];
    static int nzfac;

    int axis_ids[CMOR_MAX_DIMENSIONS], num_axis_ids;
    GT3_File *fp;
    GT3_HEADER head;
    struct file_iterator it;
    int stat;
    int rval = -1;
    int *ref_varid;
    int cal;

    fp = safe_open_mode ? GT3_open(path) : GT3_openHistFile(path);
    if (fp == NULL) {
        GT3_printErrorMessages(stderr);
        return -1;
    }

    if (time_seq)
        reinitSeq(time_seq, 1, 0x7fffffff);

    setup_file_iterator(&it, fp, time_seq);

    /*
     * skip chunks out of range.
     */
    while ((stat = iterate_file(&it)) == ITER_OUTRANGE)
        ;
    if (stat != ITER_CONTINUE) {
        logging(LOG_ERR, "No data in %s.", path);
        goto finish;
    }

    if (varname) {
        int shape[3];

        vdef = (varcnt == 1)
            ? lookup_vardef(varname)
            : lookup_formula_vardef(varname);
        if (vdef == NULL) {
            logging(LOG_ERR, "%s: No such variable in MIP table.", varname);
            goto finish;
        }

        GT3_freeVarbuf(vbuf);
        vbuf = NULL;
        free_var(var);
        var = NULL;

        if (GT3_readHeader(&head, fp) < 0
            || (vbuf = GT3_getVarbuf(fp)) == NULL) {
            GT3_printErrorMessages(stderr);
            goto finish;
        }
        if (edit_header(&head) < 0)
            goto finish;
        if ((var = new_var()) == NULL)
            goto finish;

        var->timedepend = check_timedependency(vdef);

        if (varcnt == 1) {
            char *zfattr;

            logging(LOG_INFO, "deflate level = %d, shuffle = %d",
                    deflate_level, shuffle);

            if (var->timedepend > 0 && get_calendar() == GT3_CAL_DUMMY) {
                /*
                 * set calendar automatically.
                 */
                if ((cal = GT3_guessCalendarFile(path)) < 0) {
                    GT3_printErrorMessages(stderr);
                    goto finish;
                }
                if (cal == GT3_CAL_DUMMY) {
                    logging(LOG_ERR, "cannot guess calendar.");
                    goto finish;
                }
                if (set_calendar(cal) < 0)
                    goto finish;
            }

            if (sites && update_site_indexes(sites, &head) < 0)
                goto finish;

            if (setup_axes(axis_ids, &num_axis_ids, vdef, &head) < 0
                || (varid = setup_variable(axis_ids, num_axis_ids,
                                           vdef, vbuf, &head)) < 0)
                goto finish;

            nzfac = 0;
            if ((zfattr = required_zfactors(varid))) {
                logging(LOG_INFO, "required zfactors: %s", zfattr);

                if (axis_slice[2])
                    rewindSeq(axis_slice[2]);
                if ((nzfac = setup_zfactors(zfac_ids, varid,
                                            axis_ids, num_axis_ids,
                                            &head, axis_slice[2])) < 0)
                    goto finish;
            }
            main_varid = varid;
        } else {
            /*
             * zfactors such as ps, eta, and depth.
             */
            if ((varid = lookup_varid(vdef->out_name)) < 0) {
                logging(LOG_ERR, "%s: Not ready for zfactor.", varname);
                goto finish;
            }

            if (iarray_find_first(zfac_ids, nzfac, varid) < 0) {
                logging(LOG_ERR, "%s: Not a zfactor.", varname);
                goto finish;
            }
            logging(LOG_INFO, "use %s(id=%d) as zfactor.", varname, varid);
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

        /* alloc data buffer for site data. */
        if (sites && sites->nlocs * shape[2] > site_databuf_capacity) {
            size_t siz;
            float *ptr;

            siz = sites->nlocs * shape[2];
            if ((ptr = malloc(sizeof(float) * siz)) == NULL) {
                logging(LOG_SYSERR, NULL);
                goto finish;
            }
            free(site_databuf);

            site_databuf = ptr;
            site_databuf_capacity = siz;
        }

        if (var->timedepend > 0) {
            if (check_basetime() < 0) {
                logging(LOG_ERR, "invalid basetime.");
                goto finish;
            }

            /*
             * first DATE1 and DATE2.
             */
            if (   GT3_decodeHeaderDate(&date1, &head, "DATE1") < 0
                || GT3_decodeHeaderDate(&date2, &head, "DATE2") < 0) {
                GT3_printErrorMessages(stderr);
                logging(LOG_ERR, "missing DATE1 and/or DATE2.");
                goto finish;
            }

            if (varcnt == 1) {
                const_interval = get_interval(&intv, vdef) == 0;
                date0 = date1;  /* very first date */
                logging(LOG_INFO,
                        "Date of the first: %d-%02d-%02d %02d:%02d:%02d",
                        date0.year, date0.mon, date0.day,
                        date0.hour, date0.min, date0.sec);

                if (var->timedepend == TIME_POINT
                    && GT3_cmpDate2(&date1, &date2) != 0)
                    logging(LOG_WARN,
                            "'DATE2 - DATE1' must be zero"
                            " for instantaneous data.");
            }
            if (varcnt > 1 && GT3_cmpDate2(&date0, &date1) != 0) {
                logging(LOG_ERR, "mismatch the first date in %s.", fp->path);
                goto finish;
            }
        }
    } else {
        assert(vbuf != NULL);
        if (GT3_reattachVarbuf(vbuf, fp) < 0)
            goto finish;
    }

    ref_varid = varcnt == 1 ? NULL : &main_varid;

    rewind_file_iterator(&it);
    while ((stat = iterate_file(&it)) != ITER_END) {
        if (stat == ITER_ERROR || stat == ITER_ERRORCHUNK)
            goto finish;
        if (stat == ITER_OUTRANGE)
            continue;

        if (GT3_readHeader(&head, fp) < 0) {
            GT3_printErrorMessages(stderr);
            goto finish;
        }
        if (   var->dimlen[0] != fp->dimlen[0]
            || var->dimlen[1] != fp->dimlen[1]) {
            logging(LOG_ERR, "Array shape has changed.");
            goto finish;
        }

        if (var->timedepend > 0) {
            if (const_interval) {
                if (   cmp_date(&head, "DATE1", &date1) != 0
                    || cmp_date(&head, "DATE2", &date2) != 0) {

                    logging(LOG_ERR, "invalid DATE[12] in %s (No.%d).",
                            fp->path, fp->curr + 1);
                    goto finish;
                }
            } else {
                if (   GT3_decodeHeaderDate(&date1, &head, "DATE1") < 0
                    || GT3_decodeHeaderDate(&date2, &head, "DATE2") < 0) {
                    GT3_printErrorMessages(stderr);
                    logging(LOG_ERR, "in %s (No.%d).", fp->path, fp->curr + 1);
                    goto finish;
                }
            }
            var->timebnd[0] = get_time(&date1);
            var->timebnd[1] = get_time(&date2);

            if (var->timedepend == TIME_CLIM) {
                GT3_Date date;

                GT3_decodeHeaderDate(&date, &head, "DATE");
                var->time = get_time(&date);
            } else
                var->time = .5 * (var->timebnd[0] + var->timebnd[1]);
        }

        if (axis_slice[2])
            rewindSeq(axis_slice[2]);

        if (read_var(var, vbuf, axis_slice[2]) < 0
            || (calc_expression && eval_calc(calc_expression,
                                             var->data,
                                             vbuf->miss,
                                             var->nelems) < 0)
            || write_var(varid, var, ref_varid) < 0) {
            goto finish;
        }

        if (var->timedepend > 0 && const_interval) {
            step_time(&date1, &intv);
            step_time(&date2, &intv);
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
    tdep = check_timedependency(vdef);
    assert(tdep == TIME_MEAN);

    printf("test_converter(): DONE\n");
    return 0;
}
#endif /* TEST_MAIN2 */
