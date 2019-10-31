/*
 * site.c
 */
#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#include "gtool3.h"
#include "internal.h"
#include "logging.h"
#include "myutils.h"
#include "site.h"


static site_locations *
new_site_locations(int num)
{
    site_locations *p;

    if ((p = malloc(sizeof(site_locations))) == NULL) {
        logging(LOG_SYSERR, NULL);
        return NULL;
    }
    p->nlocs = num;
    p->ids = NULL;
    p->lons = NULL;
    p->lats = NULL;
    p->grid_lons = NULL;
    p->grid_lats = NULL;
    p->indexes = NULL;

    if ((p->ids = malloc(sizeof(int) * num)) == NULL
        || (p->lons = malloc(sizeof(double) * num)) == NULL
        || (p->lats = malloc(sizeof(double) * num)) == NULL
        || (p->grid_lons = malloc(sizeof(double) * num)) == NULL
        || (p->grid_lats = malloc(sizeof(double) * num)) == NULL
        || (p->indexes = malloc(sizeof(int) * num)) == NULL) {
        logging(LOG_SYSERR, NULL);
        free_site_locations(p);
        return NULL;
    }
    return p;
}


void
free_site_locations(site_locations *locs)
{
    if (locs) {
        free(locs->indexes);
        free(locs->grid_lats);
        free(locs->grid_lons);
        free(locs->lats);
        free(locs->lons);
        free(locs->ids);
        free(locs);
    }
}


/*
 * Load site locations from a file.
 *
 * See `cmip6-cfsites-locations.txt` in
 * http://www.geosci-model-dev.net/10/359/2017/gmd-10-359-2017-supplement.zip
 */
site_locations *
load_site_locations(const char *path)
{
    site_locations *locs = NULL;
    FILE *fp;
    ssize_t cnt;
    char buf[32];
    int num;
    char *endptr;
    int i = 0, errflag = 1;

    if ((fp = fopen(path, "r")) == NULL) {
        logging(LOG_SYSERR, path);
        return NULL;
    }

    /*
     * Count up the number of site locations.
     */
    num = 0;
    while ((cnt = fskim(NULL, 0, '\n', fp)) > 0)
        num++;
    if (cnt < 0) {
        logging(LOG_SYSERR, path);
        goto finish;
    }

    if ((locs = new_site_locations(num)) == NULL)
        goto finish;

    logging(LOG_INFO, "%s: # of locations: %d", path, locs->nlocs);

    /*
     * Read each site location line by line.
     */
    fseeko(fp, 0, SEEK_SET);
    for (i = 0; i < locs->nlocs; i++) {
        /* id */
        cnt = fskim(buf, sizeof buf, ',', fp);
        locs->ids[i] = strtol(buf, &endptr, 10);
        if (endptr == buf || !(*endptr == ',' || isspace(*endptr)))
            goto finish;

        /* longitude */
        cnt = fskim(buf, sizeof buf, ',', fp);
        locs->lons[i] = strtod(buf, &endptr);
        if (endptr == buf || !(*endptr == ',' || isspace(*endptr)))
            goto finish;

        /* latitude */
        cnt = fskim(buf, sizeof buf, '\n', fp);
        locs->lats[i] = strtod(buf, &endptr);
        if (endptr == buf || !(*endptr == ',' || isspace(*endptr)))
            goto finish;
    }
    errflag = 0;

finish:
    fclose(fp);
    if (errflag) {
        logging(LOG_ERR, "%s: Invalid line at %d (%s)", path, i + 1, buf);
        free_site_locations(locs);
        locs = NULL;
    }
    return locs;
}


static int
nearest_index(const double *xs, size_t size, double x)
{
    size_t i, j = 0;
    double minval = HUGE_VAL;
    double d;

    for (i = 0; i < size; i++) {
        d = fabs(xs[i] - x);
        if (d < minval) {
            j = i;
            minval = d;
        }
    }
    return (int)j;
}


static int
nearest_index_modulo(const double *xs, size_t size, double x, double modulo)
{
    size_t i, j = 0;
    double minval = HUGE_VAL;
    double d;

    for (i = 0; i < size; i++) {
        d = fmod(fabs(x - xs[i]), modulo);
        d = fmin(d, modulo - d);

        if (d < minval) {
            j = i;
            minval = d;
        }
    }
    return (int)j;
}


int
update_site_indexes(site_locations *sites, const GT3_HEADER *head)
{
    gtool3_dim_prop prop1;
    gtool3_dim_prop prop2;
    GT3_Dim *dim1 = NULL;
    GT3_Dim *dim2 = NULL;
    double *lons;
    double *lats;
    int nlons, nlats;
    int i, ii, jj;
    int rc = -1;

    get_dim_prop(&prop1, head, 0);
    get_dim_prop(&prop2, head, 1);

    if ((dim1 = GT3_getDim(prop1.aitm)) == NULL
        || (dim2 = GT3_getDim(prop2.aitm)) == NULL) {
        goto finish;
    }

    lons = dim1->values + prop1.astr - 1;
    lats = dim2->values + prop2.astr - 1;
    nlons = prop1.aend - prop1.astr + 1;
    nlats = prop2.aend - prop2.astr + 1;

    for (i = 0; i < sites->nlocs; i++) {
        ii = nearest_index_modulo(lons, nlons, sites->lons[i], 360.);
        jj = nearest_index(lats, nlats, sites->lats[i]);

        logging(LOG_INFO, "site: %4d (%10.3f, %10.3f) -> (%12.4f, %12.4f)",
                sites->ids[i], sites->lons[i], sites->lats[i],
                lons[ii], lats[jj]);

        sites->grid_lons[i] = lons[ii];
        sites->grid_lats[i] = lats[jj];
        sites->indexes[i] = jj * nlons + ii;
    }
    rc = 0;

finish:
    GT3_freeDim(dim1);
    GT3_freeDim(dim2);
    return rc;
}
