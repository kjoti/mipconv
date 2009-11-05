#include <assert.h>
#include <stdio.h>

#include "gtool3.h"
#include "logging.h"
#include "var.h"

#include "cmor.h"
#include "cmor_supp.h"
#include "utils.h"
#include "internal.h"


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
          GT3_File *fp)
{
    int timedepend = 0;
    int varid;
    int status;
    int i, ndims, axis[7];
    char title[64], unit[32];
    GT3_HEADER head;
    double miss;
    cmor_axis_def_t *axisdef;


    if (GT3_readHeader(&head, fp) < 0) {
        GT3_printErrorMessages(stderr);
        return -1;
    }

    title[0] = unit[0] = '\0';
    GT3_copyHeaderItem(title, sizeof title, &head, "TITLE");
    GT3_copyHeaderItem(unit, sizeof unit, &head, "UNIT");

    translate_unit(unit, sizeof unit);

    /*
     *  set axes.
     */
    for (i = 0, ndims = vdef->ndims; i < vdef->ndims; i++) {
        axisdef = get_axisdef_in_vardef(vdef, i);

        /* Singleton axis can be ignored here. */
        if (is_singleton(axisdef)) {
            ndims--;
            continue;
        }
        if (axisdef->axis == 'T') {
            timedepend = 1;
            ndims--;
            continue;
        }
    }
    if (ndims <= 3) {
        for (i = 0; i < ndims; i++)
            axis[i] = get_axis_by_gtool3(&head, i);
    } else {
        /*
         *  We need some trick because gtool3 can have up to three axes.
         */
        assert(!"Not supported yet");
    }

    if (timedepend)
        axis[ndims] = get_timeaxis();

    /* lon, lat, lev, time => time, lev, lat, lon. */
    reverse_iarray(axis, ndims + timedepend);

#if 0
    ndims = 3;
    axis[2] = get_axis_by_gt3name("GLON128", 1, 128);
    axis[1] = get_axis_by_gt3name("GGLA64", 1, 64);
#endif

    miss = vbuf->miss;
    status = cmor_variable(&varid, (char *)vdef->id, unit,
                           ndims + timedepend, axis,
                           'd', &miss,
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


int
convert(const char *varname, const char *path, int cnt)
{
    GT3_File *fp;
    static GT3_Varbuf *vbuf = NULL;
    static int varid;
    cmor_var_def_t *vdef;
    static myvar_t *var = NULL;


    if ((fp = GT3_open(path)) == NULL) {
        GT3_printErrorMessages(stderr);
        return -1;
    }

    if (cnt == 0) {
        if ((vdef = lookup_vardef(varname, NULL)) == NULL) {
            logging(LOG_ERR, "%s: No such variable in MIP table", varname);
            GT3_close(fp);
            return -1;
        }

        GT3_freeVarbuf(vbuf);
        vbuf = NULL;
        free_var(var);
        var = NULL;

        if ((vbuf = GT3_getVarbuf(fp)) == NULL) {
            GT3_printErrorMessages(stderr);
            GT3_close(fp);
            return -1;
        }

        if ((varid = get_varid(vdef, vbuf, fp)) < 0
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
        if (read_var(var, vbuf) < 0 || write_var(varid, var) < 0) {
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
