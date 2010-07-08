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


static GT3_Dim *
load_dim(const char *fmt, const char *aitm, int len_check)
{
    GT3_Dim *dim;
    char name[17];

    snprintf(name, sizeof name, fmt, aitm);
    if ((dim = GT3_getDim(name)) == NULL)
        GT3_printErrorMessages(stderr);

    if (dim && dim->len != len_check) {
        logging(LOG_ERR, "%s: unexpendted dim length", name);

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

    if (   (a_bnd = load_dim("%s_A.M", aitm, dimlen + 1)) == NULL
        || (b_bnd = load_dim("%s_B.M", aitm, dimlen + 1)) == NULL
        || (a = load_dim("%s_A", aitm, dimlen)) == NULL
        || (b = load_dim("%s_B", aitm, dimlen)) == NULL)
        goto finish;

    /* p0: reference pressure: 100000.0 Pa */
    p0 = 1e5;
    if (cmor_zfactor(&p0_id, sigid, "p0", "Pa",
                     0, NULL, 'd', &p0, NULL) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor: p0: id = %d", p0_id);

    /*
     * "hPa -> Pa" and "/ p0".
     */
    for (i = 0; i < a_bnd->len; i++)
        a_bnd->values[i] *= 100. / p0;
    for (i = 0; i < a->len; i++)
        a->values[i] *= 100. / p0;

    /* a */
    if (cmor_zfactor(&a_id, sigid, "a", " ",
                     1, &sigid, 'd',
                     a->values + astr - 1,
                     a_bnd->values + astr - 1) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor: a:  id = %d", a_id);

    /* b */
    if (cmor_zfactor(&b_id, sigid, "b", " ",
                     1, &sigid, 'd',
                     b->values + astr -1,
                     b_bnd->values + astr - 1) != 0)
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


static int
check_vrange(const double *values, size_t nelems, double low, double high)
{
    int i;

    for (i = 0; i < nelems; i++)
        if (values[i] < low || values[i] > high)
            return -1;

    return 0;
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
    double *sigma = NULL, *sigma_bnd = NULL;
    double depth_c = 38.0;     /* ZBOT[m] */
    char bndname[17];
    int rval = -1;
    int sigma_id, depth_c_id, nsigma_id, zlev_id;
    int nsigma, len;
    int i;

    snprintf(bndname, sizeof bndname, "%s.M", aitm);
    if ((dim = GT3_getDim(aitm)) == NULL
        || (bnd = GT3_getDim(bndname)) == NULL) {
        GT3_printErrorMessages(stderr);
        goto finish;
    }

    len = aend - astr + 1;
    if ((sigma = malloc(sizeof(double) * len)) == NULL
        || (sigma_bnd = malloc(sizeof(double) * (len + 1))) == NULL) {
        logging(LOG_SYSERR, NULL);
        goto finish;
    }

    /*
     * Calculates sigma from depth and depth_c(ZBOT).
     *
     * XXX Only 'nsigma' elements of sigma are meaingful.
     */
    for (i = 0; i < len; i++)
        sigma[i] = -dim->values[i + astr - 1] / depth_c;
    for (i = 0; i < len + 1; i++)
        sigma_bnd[i] = -bnd->values[i + astr - 1] / depth_c;

    for (nsigma = 0; nsigma < len && sigma_bnd[nsigma + 1] >= -1.; nsigma++)
        ;
    logging(LOG_INFO, "# of sigma-level in ocean_sigma_z: %d", nsigma);

    if (check_vrange(sigma, nsigma, -1.0, 0.0) < 0
        || check_vrange(sigma_bnd, nsigma + 1, -1.0, 0.0) < 0) {
        logging(LOG_ERR, "invalid ocean_sigma value");
        return -1;
    }

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
    if (cmor_zfactor(&sigma_id, z_id, "sigma", "1",
                     1, &z_id, 'd',
                     sigma, sigma_bnd) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor: sigma:   id = %d", sigma_id);

    /* zlev */
    if (cmor_zfactor(&zlev_id, z_id, "zlev", " ", /* dim->unit, */
                     1, &z_id, 'd',
                     dim->values + astr - 1,
                     bnd->values + astr - 1) != 0)
        goto finish;
    logging(LOG_INFO, "zfactor: zlev:    id = %d", zlev_id);

    rval = 0;
finish:
    free(sigma_bnd);
    free(sigma);
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
    int aid;
    int axes_ids[7], naxes;
    int axes_ids2[7], naxes2;
    int i;
    int z_id = -1;
    struct {
        char *name;
        char *unit;
        char type;
        int naxes;
        int *axes_ids;
    } zfactors[8];
    gtool3_dim_prop dim;

    /*
     * collect axis-ids except for Z-axis.
     */
    for (i = 0, naxes = naxes2 = 0; i < cmor_vars[var_id].ndims; i++) {
        aid = cmor_vars[var_id].axes_ids[i];

        if (strchr("XYT", cmor_axes[aid].axis)) {
            axes_ids[naxes] = aid;
            naxes++;
        }
        if (strchr("XY", cmor_axes[aid].axis)) {
            axes_ids2[naxes2] = aid;
            naxes2++;
        }
        if (cmor_axes[aid].axis == 'Z')
            z_id = aid;
    }
    if (z_id == -1)
        return 0; /* This variable has no Z-axis. */


    for (i = 0; i < sizeof zfactors / sizeof zfactors[0]; i++) {
        zfactors[i].name = NULL;

        /* default setting */
        zfactors[i].naxes = naxes;
        zfactors[i].axes_ids = axes_ids;
        zfactors[i].type = 'd';
    }

    /*
     * XXX: The unit of zfactors is hard-coded.
     */
    for (i = 0; i < 3; i++) {
        get_dim_prop(&dim, head, 2 - i);

        if (startswith(dim.aitm, "CSIG")) {
            if (std2_sigma(z_id, dim.aitm, dim.astr, dim.aend) < 0)
                return -1;
            zfactors[0].name = "ps"; /* surface pressure */
            zfactors[0].unit = "hPa";
            break;
        }

        if (startswith(dim.aitm, "HETA")) {
            if (hyb_sigma(z_id, dim.aitm, dim.astr, dim.aend) < 0)
                return -1;
            zfactors[0].name = "ps"; /* surface pressure */
            zfactors[0].unit = "hPa";
            break;
        }

        if (startswith(dim.aitm, "OCDEP")) {
            if (ocean_sigma(z_id, dim.aitm, dim.astr, dim.aend) < 0)
                return -1;

            /* XXX: The order (eta, depth) is significant. */
            zfactors[0].name = "eta"; /* sea surface height */
            zfactors[0].unit = "cm";

            zfactors[1].name = "depth"; /* sea floor depth */
            zfactors[1].unit = "m";
            zfactors[1].naxes = naxes2;
            zfactors[1].axes_ids = axes_ids2;
            break;
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
        zfac_ids[i] = temp_id;
    }
    return i;
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
