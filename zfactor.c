/*
 * zfactor.c
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "internal.h"
#include "cmor_supp.h"
#include "myutils.h"

/* defined in setup.c */
extern double ocean_sigma_bottom;

static double missing_value = 1e20;

/*
 * z-factors for standard_sigma.
 *
 * formula: p(n,k,j,i) = ptop + sigma(k)*(ps(n,j,i) - ptop)
 *
 * We need not call cmor_zfactor() for "sigma",
 * "sigma" is already stored as "lev".
 */
static int
std_sigma(int sigid, const char *aitm, int astr, int aend)
{
    int rval = -1;
    double ptop = 0.;           /* Ptop: 0. [Pa] */
    int ptop_id;

    /* ptop */
    if (cmor_zfactor(&ptop_id, sigid, "ptop", "Pa",
                     0, NULL, 'd', &ptop, NULL) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor:  ptop: id = %d", ptop_id);
    assert(ptop_id >= 0);
    rval = 0;

finish:
    return rval;
}


static GT3_Dim *
load_dim(const char *fmt, const char *aitm, int len_required)
{
    GT3_Dim *dim;
    char name[17];

    snprintf(name, sizeof name, fmt, aitm);
    if ((dim = GT3_getDim(name)) == NULL)
        GT3_printErrorMessages(stderr);

    if (dim && dim->len - dim->cyclic < len_required) {
        logging(LOG_ERR, "%s: unexpected dim length.", name);

        GT3_freeDim(dim);
        dim = NULL;
    }
    return dim;
}


/*
 * z-factors for HETA*.
 *
 * for formula: p(n,k,j,i) = a(k)*p0 + b(k)*ps(n,j,i)
 */
static int
hyb_sigma(int sigid, const char *aitm, int astr, int aend)
{
    int rval = -1;
    GT3_Dim *a_bnd = NULL, *b_bnd = NULL;
    GT3_Dim *a = NULL, *b = NULL;
    double p0;
    int dimlen = aend - astr + 1;
    int i, p0_id, a_id, b_id;
    char *term_name;
    double *bounds;
    double fact;

    if (cmor_axes[sigid].bounds != NULL) {
        if (   (a_bnd = load_dim("%s_A.M", aitm, dimlen + 1)) == NULL
            || (b_bnd = load_dim("%s_B.M", aitm, dimlen + 1)) == NULL)
            goto finish;
    }
    if (   (a = load_dim("%s_A", aitm, dimlen)) == NULL
        || (b = load_dim("%s_B", aitm, dimlen)) == NULL)
        goto finish;

    /* p0: reference pressure: 100000.0 Pa */
    p0 = 1e5;
    if (cmor_zfactor(&p0_id, sigid, "p0", "Pa",
                     0, NULL, 'd', &p0, NULL) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor: p0: id = %d", p0_id);

    /*
     * Convert FA in MIROC -> `a_bnd`, `a`
     */
    if (a_bnd) {
        fact = (strcmp(a_bnd->unit, "hPa") == 0) ? 100.0 : 1.;
        for (i = 0; i < a_bnd->len; i++)
            a_bnd->values[i] *= fact / p0;
    }
    for (i = 0; i < a->len; i++) {
        fact = (strcmp(a->unit, "hPa") == 0) ? 100.0 : 1.;
        a->values[i] *= fact / p0;
    }

    /* a */
    term_name = a_bnd ? "a" : "a_half";
    bounds = a_bnd ? a_bnd->values + astr - 1 : NULL;
    if (cmor_zfactor(&a_id, sigid, term_name, "",
                     1, &sigid, 'd',
                     a->values + astr - 1,
                     bounds) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor: a:  id = %d", a_id);

    /* b */
    term_name = b_bnd ? "b" : "b_half";
    bounds = b_bnd ? b_bnd->values + astr - 1 : NULL;
    if (cmor_zfactor(&b_id, sigid, term_name, "",
                     1, &sigid, 'd',
                     b->values + astr -1,
                     bounds) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor: b:  id = %d", b_id);

    assert(p0_id >= 0);
    assert(a_id >= 0);
    assert(b_id >= 0);
    rval = 0;
finish:
    GT3_freeDim(b);
    GT3_freeDim(a);
    GT3_freeDim(b_bnd);
    GT3_freeDim(a_bnd);
    return rval;
}


/*
 * `zlev` and `sigma` in ocean_sigma_z{,_half}.
 *
 * Note:
 *   * "positive": "up"
 *   * "stored_direction": "decreasing"
 */
static void
zlev_and_sigma_in_ocean(double *zlev, double *sigma, int length)
{
    int i;
    double z, s;

    for (i = 0; i < length; i++) {
        z = zlev[i];
        s = z / ocean_sigma_bottom;

        if (s >= -1.0)  /* upper layers */
            z = missing_value;
        else
            s = missing_value;

        zlev[i] = z;
        sigma[i] = s;
    }
}


/*
 * zfactor for ocean_sigma_z.
 *
 * for k <= nsigma:
 *     z(n,k,j,i) = eta(n,j,i) + sigma(k)*(min(depth_c,depth(j,i))+eta(n,j,i))
 * for k > nsigma:
 *     z(n,k,j,i) = zlev(k)
 */
static int
ocean_sigma_z(int z_id, const char *aitm, int astr, int aend)
{
    GT3_Dim *dim = NULL, *bnd = NULL;
    double *sigma = NULL, *sigma_bnd = NULL;
    char bndname[19];
    int sigma_id, zlev_id, vid;
    int rval = -1;

    snprintf(bndname, sizeof bndname, "%s.M", aitm);
    if ((dim = GT3_getDim(aitm)) == NULL
        || (bnd = GT3_getDim(bndname)) == NULL) {
        GT3_printErrorMessages(stderr);
        goto finish;
    }
    if (bnd->len != dim->len + 1) {
        logging(LOG_ERR, "ocean_sigma: Size mismatch");
        goto finish;
    }

    if ((sigma = malloc(sizeof(double) * dim->len)) == NULL
        || (sigma_bnd = malloc(sizeof(double) * (bnd->len))) == NULL) {
        logging(LOG_SYSERR, NULL);
        goto finish;
    }

    /*
     * zlev and sigma for ocean_sigma_z.
     */
    zlev_and_sigma_in_ocean(dim->values, sigma, dim->len);
    zlev_and_sigma_in_ocean(bnd->values, sigma_bnd, bnd->len);

    /*
     * sigma and sigma_bnds [1]
     */
    if (cmor_zfactor(&sigma_id, z_id, "sigma", "1",
                     1, &z_id, 'd',
                     sigma, sigma_bnd) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor: sigma: id = %d", sigma_id);

    set_missing_value(sigma_id, missing_value);
    vid = lookup_varid("sigma_bnds");
    assert(vid >= 0);
    set_missing_value(vid, missing_value);

    /*
     * zlev and zlev_bnds [m]
     */
    if (cmor_zfactor(&zlev_id, z_id, "zlev", "m",
                     1, &z_id, 'd',
                     dim->values + astr - 1,
                     bnd->values + astr - 1) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor: zlev: id = %d", zlev_id);

    set_missing_value(zlev_id, missing_value);
    vid = lookup_varid("zlev_bnds");
    assert(vid >= 0);
    set_missing_value(vid, missing_value);

    rval = 0;
finish:
    free(sigma_bnd);
    free(sigma);
    GT3_freeDim(dim);
    GT3_freeDim(bnd);
    return rval;
}


/*
 * Search the corresponding formula term from candidates
 * by comparing the dimensions (e.g., time, time1, time2).
 *
 * e.g.,
 *     search ps from 'ps', 'ps1' and 'ps2'.
 *     search eta from  'eta' and 'eta2'.
 */
static int
search_formula_term(const char * const candidates[], int num_candidates,
                    const int *axes_ids, int naxes)
{
    cmor_var_def_t *formula;
    int i, j, j2;

    for (i = 0; i < num_candidates; i++) {
        if ((formula = lookup_formula_vardef(candidates[i])) == NULL)
            continue;

        for (j = 0, j2 = 0; j < naxes && j2 < CMOR_MAX_DIMENSIONS; j++, j2++) {
            if (axes_ids[j] < 0) { /* grid id */
                j2++;
                continue;
            }
            if (cmor_axes[axes_ids[j]].ref_axis_id != formula->dimensions[j2])
                break;
        }
        if (j == naxes) {
            logging(LOG_INFO, "search_formula_term: Found %s", candidates[i]);
            return i;
        }
    }
    return -1;
}


/*
 * set all the zfactors for specified 'var_id'.
 */
int
setup_zfactors(int var_id, const int *vaxis_ids, int nvaxes,
               const generic_axis *glev,
               const struct sequence *zslice)
{
    int z_id = -1;
    int aid;
    int axes_ids[7], naxes;    /* [T, Y, X] */
    int axes_ids2[7], naxes2;  /* [Y, X] */
    int i;
    struct {
        char *name;
        char *unit;
        char type;
        int naxes;
        int *axes_ids;
    } zfactors[16];

    /*
     * collect axis-ids except for Z-axis.
     */
    for (i = 0, naxes = naxes2 = 0; i < nvaxes; i++) {
        aid = vaxis_ids[i];  /* XXX: aid < 0 if grid_id. */

        if (aid >= 0 && cmor_axes[aid].axis == 'Z')
            z_id = aid;
        else {
            axes_ids[naxes] = aid;
            naxes++;
            if (aid < 0 || cmor_axes[aid].axis != 'T') {
                axes_ids2[naxes2] = aid;
                naxes2++;
            }
        }
    }
    if (z_id == -1) {
        logging(LOG_WARN, "zfactors are required but no z-axis?");
        return 0;
    }

    for (i = 0; i < sizeof zfactors / sizeof zfactors[0]; i++) {
        zfactors[i].name = NULL;

        /* default setting */
        zfactors[i].naxes = naxes;
        zfactors[i].axes_ids = axes_ids;
        zfactors[i].type = 'd';
    }

    /*
     * Generic level coordinates (e.g., alevel, olevel, ...)
     */
    if (glev->id[0] != '\0') {
        int astr, aend, step;

        astr = glev->astr;
        aend = glev->aend;
        if (zslice) {
            checkSeq(zslice, &astr, &aend, &step);
            if (step != 1) {
                logging(LOG_ERR, "%s: cannot slice this axis.", glev->aitm);
                return -1;
            }
        }
        if (startswith(glev->id, "standard_sigma")) {
            const char * const names[] = {"ps", "ps1", "ps2", "ps2d"};
            int num = sizeof names / sizeof names[0];
            int idx;

            if (std_sigma(z_id, glev->aitm, astr, aend) < 0)
                return -1;

            /* search the formula term (zfactor) */
            idx = search_formula_term(names, num, axes_ids, naxes);
            if (idx < 0) {
                logging(LOG_ERR, "setup_zfactors: Not found ps");
                return -1;
            }
            zfactors[0].name = (char *)names[idx];
            zfactors[0].unit = "hPa";
        } else if (startswith(glev->id, "standard_hybrid_sigma")) {
            const char * const names[] = {"ps", "ps1", "ps2", "ps2d"};
            int num = sizeof names / sizeof names[0];
            int idx;

            if (hyb_sigma(z_id, glev->aitm, astr, aend) < 0)
                return -1;

            /* search the formula term (zfactor) */
            idx = search_formula_term(names, num, axes_ids, naxes);
            if (idx < 0) {
                logging(LOG_ERR, "setup_zfactors: Not found ps");
                return -1;
            }

            zfactors[0].name = (char *)names[idx];
            zfactors[0].unit = "hPa";
        } else if (startswith(glev->id, "ocean_sigma_z")) {
            const char * const names[] = {"eta", "eta2"};
            int num = sizeof names / sizeof names[0];
            int depth_c_id, idx;

            /* depth_c */
            if (cmor_zfactor(&depth_c_id, z_id, "depth_c", "m",
                             0, NULL, 'd', &ocean_sigma_bottom, NULL) != 0) {
                return -1;
            }
            logging(LOG_INFO, "zfactor: depth_c: id = %d", depth_c_id);

            if (ocean_sigma_z(z_id, glev->aitm, astr, aend) < 0)
                return -1;

            /* search the formula term (zfactor) */
            idx = search_formula_term(names, num, axes_ids, naxes);
            if (idx < 0) {
                logging(LOG_ERR, "setup_zfactors: Not found eta");
                return -1;
            }

            zfactors[0].name = (char *)names[idx];
            zfactors[0].unit = "cm";

            zfactors[1].name = "depth"; /* sea floor depth */
            zfactors[1].unit = "m";
            zfactors[1].naxes = naxes2;
            zfactors[1].axes_ids = axes_ids2;
        }
    }

    /*
     * call cmor_zfactor() for each zfactor.
     */
    for (i = 0; zfactors[i].name != NULL; i++) {
        int temp_id;

        if (cmor_zfactor(&temp_id, z_id,
                         zfactors[i].name, zfactors[i].unit,
                         zfactors[i].naxes, zfactors[i].axes_ids,
                         zfactors[i].type,
                         NULL, NULL) != 0)
            return -1;

        logging(LOG_INFO, "zfactor: %s: id = %d", zfactors[i].name, temp_id);
    }
    return 0;
}
