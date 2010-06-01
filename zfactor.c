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


/*
 * z-factors for standard_sigma.
 * XXX: This function DOES NOT WORK in current CMOR2
 * because of missing sigma definition.
 *
 * formula: p(n,k,j,i) = ptop + sigma(k)*(ps(n,j,i) - ptop)
 */
static int
std_sigma(int sigid, const char *aitm, int astr, int aend)
{
    int rval = -1;
    char name[17];
    GT3_Dim *dim = NULL, *bnd = NULL;
    double ptop = 0.;
    int ptop_id, sigma_id;

    snprintf(name, sizeof name, "%s.M", aitm);
    if (   (dim = GT3_getDim(aitm)) == NULL
        || (bnd = GT3_getDim(name)) == NULL) {
        GT3_printErrorMessages(stderr);
        goto finish;
    }

    /* ptop */
    if (cmor_zfactor(&ptop_id, sigid, "ptop", "Pa",
                     0, NULL, 'd', &ptop, NULL) != 0)
        goto finish;

    /* sigma */
    if (cmor_zfactor(&sigma_id, sigid, "sigma", "",
                     1, &sigid, 'd',
                     dim->values + astr - 1,
                     bnd->values + astr - 1) != 0)
        goto finish;

    logging(LOG_INFO, "zfactor:  ptop: id = %d", ptop_id);
    logging(LOG_INFO, "zfactor: sigma: id = %d", sigma_id);
    assert(ptop_id >= 0);
    assert(sigma_id >= 0);
    rval = 0;

finish:
    GT3_freeDim(bnd);
    GT3_freeDim(dim);
    return rval;
}


/*
 * z-factors for standard_hybrid_sigma.
 * CSIG.
 *
 * formula: p(n,k,j,i) = a(k)*p0 + b(k)*ps(n,j,i)
 */
static int
std2_sigma(int sigid, const char *aitm, int astr, int aend)
{
    int rval = -1;
    char name[17];
    GT3_Dim *dim = NULL, *bnd = NULL;
    double p0;
    double *a_bnd = NULL;
    int dimlen = aend - astr + 1;
    int i, p0_id, a_id, b_id;

    snprintf(name, sizeof name, "%s.M", aitm);
    if ((dim = GT3_getDim(aitm)) == NULL
        || (bnd = GT3_getDim(name)) == NULL) {
        GT3_printErrorMessages(stderr);
        rval = -1;
        goto finish;
    }

    if ((a_bnd = malloc(sizeof(double) * (dimlen + 1))) == NULL) {
        logging(LOG_SYSERR, NULL);
        rval = -1;
        goto finish;
    }
    for (i = 0; i < dimlen + 1; i++)
        a_bnd[i] = 0.;

    /* p0 */
    p0 = 1e5;
    if (cmor_zfactor(&p0_id, sigid, "p0", "Pa",
                     0, NULL, 'd', &p0, NULL) != 0)
        goto finish;

    /* a (zero) */
    if (cmor_zfactor(&a_id, sigid, "a", "",
                     1, &sigid, 'd', a_bnd, a_bnd) != 0)
        goto finish;

    /* b */
    if (cmor_zfactor(&b_id, sigid, "b", "",
                     1, &sigid, 'd',
                     dim->values + astr - 1,
                     bnd->values + astr - 1) != 0)
        goto finish;

    logging(LOG_INFO, "zfactor: p0: id = %d", p0_id);
    logging(LOG_INFO, "zfactor: a:  id = %d", a_id);
    logging(LOG_INFO, "zfactor: b:  id = %d", b_id);

    assert(p0_id >= 0);
    assert(a_id >= 0);
    assert(b_id >= 0);
    rval = 0;
finish:
    free(a_bnd);
    GT3_freeDim(bnd);
    GT3_freeDim(dim);
    return rval;
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
    char name1[17], name2[17];
    GT3_Dim *a_bnd = NULL, *b_bnd = NULL;
    double *mid = NULL;
    double p0;
    int dimlen = aend - astr + 1;
    int i, j, p0_id, a_id, b_id;


    snprintf(name1, sizeof name1, "%s_A.M", aitm);
    snprintf(name2, sizeof name2, "%s_B.M", aitm);
    if (   (a_bnd = GT3_getDim(name1)) == NULL
        || (b_bnd = GT3_getDim(name2)) == NULL) {
        GT3_printErrorMessages(stderr);
        goto finish;
    }
    if (a_bnd->len != dimlen + 1) {
        logging(LOG_ERR, "%s: unexpendted dim length", name1);
        goto finish;
    }
    if (b_bnd->len != dimlen + 1) {
        logging(LOG_ERR, "%s: unexpendted dim length", name2);
        goto finish;
    }
    if ((mid = malloc(sizeof(double) * dimlen)) == NULL) {
        logging(LOG_SYSERR, NULL);
        goto finish;
    }

    /* p0: reference pressure: 100000.0 Pa */
    p0 = 1e5;
    if (cmor_zfactor(&p0_id, sigid, "p0", "Pa",
                     0, NULL, 'd', &p0, NULL) != 0)
        goto finish;
    for (i = 0; i < a_bnd->len; i++) {
        a_bnd->values[i] *= 100.0; /* hPa -> Pa */
        a_bnd->values[i] /= p0;
    }
    /* a */
    for (i = 0, j = astr - 1; i < dimlen; i++, j++)
        mid[i] = .5 * (a_bnd->values[j] + a_bnd->values[j+1]);
    if (cmor_zfactor(&a_id, sigid, "a", " ",
                     1, &sigid, 'd',
                     mid,
                     a_bnd->values + astr - 1) != 0)
        goto finish;

    /* b */
    for (i = 0, j = astr - 1; i < dimlen; i++, j++)
        mid[i] = .5 * (b_bnd->values[j] + b_bnd->values[j+1]);
    if (cmor_zfactor(&b_id, sigid, "b", " ",
                     1, &sigid, 'd',
                     mid,
                     b_bnd->values + astr - 1) != 0)
        goto finish;

    logging(LOG_INFO, "zfactor: p0: id = %d", p0_id);
    logging(LOG_INFO, "zfactor: a:  id = %d", a_id);
    logging(LOG_INFO, "zfactor: b:  id = %d", b_id);

    assert(p0_id >= 0);
    assert(a_id >= 0);
    assert(b_id >= 0);
    rval = 0;
finish:
    free(mid);
    GT3_freeDim(b_bnd);
    GT3_freeDim(a_bnd);
    return rval;
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
ocean_sigma(int z_id, const char *aitm, int astr, int aend)
{
    GT3_Dim *dim = NULL, *bnd = NULL;
    char bndname[17];
    int rval = -1;
    int sigma_id, depth_c_id, nsigma_id, zlev_id;
#define NSIGMA 8
    int nsigma = NSIGMA;
    double sigma[NSIGMA], sigma_bnd[NSIGMA+1];
    double depth_c = 38.0;     /* ZBOT[m] */
    int i;

    snprintf(bndname, sizeof bndname, "%s.M", aitm);
    if ((dim = GT3_getDim(aitm)) == NULL
        || (bnd = GT3_getDim(bndname)) == NULL) {
        GT3_printErrorMessages(stderr);
        goto finish;
    }

    /*
     * calculates sigma from depth and depth_c(ZBOT).
     */
    for (i = 0; i < nsigma; i++)
        sigma[i] = -dim->values[i] / depth_c;
    for (i = 0; i < nsigma + 1; i++)
        sigma_bnd[i] = -bnd->values[i] / depth_c;

    /* depth_c */
    if (cmor_zfactor(&depth_c_id, z_id, "depth_c", "m",
                     0, NULL, 'd', &depth_c, NULL) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor: depth_c: id = %d", depth_c_id);

    /* nsigma */
    if (cmor_zfactor(&nsigma_id, z_id, "nsigma", "1",
                     0, NULL, 'i', &nsigma, NULL) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor: nsigma:  id = %d", nsigma_id);

    /* sigma */
    assert(sizeof sigma / sizeof sigma[0] == nsigma);
    assert(sizeof sigma_bnd / sizeof sigma_bnd[0] == nsigma + 1);
    if (cmor_zfactor(&sigma_id, z_id, "sigma", "1",
                     1, &z_id, 'd',
                     sigma, sigma_bnd) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor: sigma:   id = %d", sigma_id);

    /* zlev */
    if (cmor_zfactor(&zlev_id, z_id, "zlev", dim->unit,
                     1, &z_id, 'd',
                     dim->values + astr - 1,
                     bnd->values + astr - 1) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor: zlev:    id = %d", zlev_id);

    rval = 0;
finish:
    GT3_freeDim(dim);
    GT3_freeDim(bnd);

    return rval;
}


/*
 * set all the zfactors for specified 'var_id'.
 *
 * Return value:
 *   o the number of z-factors such as 'ps', 'depth', 'eta',
 *     which will be read in as additional data.
 *   o -1 on error.
 *
 * Note:
 *   zfac_ids must have enough space.
 */
int
setup_zfactors(int *zfac_ids, int var_id, const GT3_HEADER *head)
{
    char aitm[17];
    int astr, aend;
    int aid, axes_ids[7], naxes;
    int i;
    int z_id = -1;
    int n_zfac = 0;
    int surface_pressure = 0;
    int ocean_depth = 0;
    int sea_surface_height = 0;

    /*
     * collect axis-ids except for Z-axis.
     */
    for (i = 0, naxes = 0; i < cmor_vars[var_id].ndims; i++) {
        aid = cmor_vars[var_id].axes_ids[i];

        if (strchr("XYT", cmor_axes[aid].axis)) {
            axes_ids[naxes] = aid;
            naxes++;
        }
        if (cmor_axes[aid].axis == 'Z')
            z_id = aid;
    }
    if (z_id == -1)
        return 0; /* This variable has no Z-axis. */

    for (i = 0; i < 3; i++) {
        get_axis_prof(aitm, &astr, &aend, head, 2 - i);

        if (startswith(aitm, "CSIG")) {
            if (std2_sigma(z_id, aitm, astr, aend) < 0)
                return -1;
            surface_pressure = 1;
            break;
        }

        if (startswith(aitm, "HETA")) {
            if (hyb_sigma(z_id, aitm, astr, aend) < 0)
                return -1;
            surface_pressure = 1;
            break;
        }

        if (startswith(aitm, "OCDEP")) {
            if (ocean_sigma(z_id, aitm, astr, aend) < 0)
                return -1;
            ocean_depth = 1;
            sea_surface_height = 1;
            break;
        }
    }

    if (surface_pressure) {
        int ps_id;

        if (cmor_zfactor(&ps_id, z_id, "ps", "hPa",
                         naxes, axes_ids, 'd', NULL, NULL) != 0)
            return -1;

        logging(LOG_INFO, "zfactor: ps: id = %d", ps_id);
        zfac_ids[n_zfac] = ps_id;
        n_zfac++;
    }

    if (ocean_depth) {
    }

    if (sea_surface_height) {
        int eta_id;

        if (cmor_zfactor(&eta_id, z_id, "eta", "cm",
                         naxes, axes_ids, 'd', NULL, NULL) != 0)
            return -1;

        logging(LOG_INFO, "zfactor: eta: id = %d", eta_id);
        zfac_ids[n_zfac] = eta_id;
        n_zfac++;
    }
    return n_zfac;
}


#ifdef TEST_MAIN2
void
test_csig(void)
{
    cmor_var_def_t *vdef;
    int rval;
    int ids[7], nid;

    vdef = lookup_vardef("cl");
    assert(vdef);
    assert(has_modellevel_dim(vdef));

    rval = get_axis_ids(ids, &nid, "CSIG20", 1, 20, NULL, vdef);
    assert(rval == 0);
    assert(nid == 1);
    assert(ids[0] >= 0);

    rval = std2_sigma(ids[0], "CSIG20", 1, 20);
    assert(rval == 0);
}


void
test_ocean(void)
{
    cmor_var_def_t *vdef;
    int rval;
    int ids[7], nid;

    vdef = lookup_vardef("uo");
    assert(vdef);
    assert(has_modellevel_dim(vdef));

    rval = get_axis_ids(ids, &nid, "OCDEPT48", 1, 48, NULL, vdef);
    assert(rval == 0);
    assert(nid == 1);

    rval = ocean_sigma(ids[0], "OCDEPT48", 1, 48);
    assert(rval == 0);
}



int
test_zfactor(void)
{
    test_csig();
    /* test_ocean(); */
    printf("test_zfactor(): DONE\n");
    return 0;
}
#endif /* TEST_MAIN2 */
