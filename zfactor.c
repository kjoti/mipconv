/*
 * zfactor.c
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "internal.h"


#if 0
/*
 * call cmor_zfactor() for CSIG*.
 * XXX: Current CMOR2 cannot treate "standard_sigma" as it is.
 *
 * set parameter, "ptop", "sigma".
 * p(n,k,j,i) = ptop + sigma(k)*(ps(n,j,i) - ptop)
 */
static int
std_sigma(int sigid, const char *aitm, int astr, int aend)
{
    int rval = -1;
    char name[17];
    GT3_Dim *dim, *bnd;
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
#endif

/*
 * call cmor_zfactor() for CSIG*.
 * XXX: This uses standard_hybrid_sigma instead of standard_sigma.
 *
 * for formula: p(n,k,j,i) = a(k)*p0 + b(k)*ps(n,j,i)
 */
static int
std2_sigma(int sigid, const char *aitm, int astr, int aend)
{
    int rval = -1;
    char name[17];
    GT3_Dim *dim, *bnd;
    double p0;
    double *a_bnd;
    int dimlen = aend - astr + 1;
    int i, p0_id, a_id, b_id;

    snprintf(name, sizeof name, "%s.M", aitm);
    if ((dim = GT3_getDim(aitm)) == NULL
        || (bnd = GT3_getDim(name)) == NULL) {
        GT3_printErrorMessages(stderr);
        return -1;
    }

    if ((a_bnd = malloc(sizeof(double) * (dimlen + 1))) == NULL) {
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
 * call cmor_zfactor() for HETA*.
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
 * set all the zfactors for specified 'var_id'.
 * return the number of z-factors.
 */
int
setup_zfactors(int *zfac_ids, int var_id, const GT3_HEADER *head)
{
    char aitm[17];
    int astr, aend;
    int axes_ids[7], sig_id;
    int ps_id;

    get_axis_prof(aitm, &astr, &aend, head, 2);

    if (strncmp(aitm, "CSIG", 4) == 0) {
        axes_ids[0] = cmor_vars[var_id].axes_ids[0]; /* time */
        axes_ids[1] = cmor_vars[var_id].axes_ids[2]; /* lat */
        axes_ids[2] = cmor_vars[var_id].axes_ids[3]; /* lon */

        sig_id = cmor_vars[var_id].axes_ids[1];

        if (std2_sigma(sig_id, aitm, astr, aend) < 0)
            return -1;

        if (cmor_zfactor(&ps_id, sig_id, "ps", "hPa",
                         3, axes_ids, 'd', NULL, NULL) != 0)
            return -1;

        logging(LOG_INFO, "zfactor: ps: id = %d", ps_id);
        zfac_ids[0] = ps_id;
        return 1;
    }

    if (strncmp(aitm, "HETA", 4) == 0) {
        axes_ids[0] = cmor_vars[var_id].axes_ids[0]; /* time */
        axes_ids[1] = cmor_vars[var_id].axes_ids[2]; /* lat */
        axes_ids[2] = cmor_vars[var_id].axes_ids[3]; /* lon */

        sig_id = cmor_vars[var_id].axes_ids[1];

        if (hyb_sigma(sig_id, aitm, astr, aend) < 0)
            return -1;

        if (cmor_zfactor(&ps_id, sig_id, "ps", "hPa",
                         3, axes_ids, 'd', NULL, NULL) != 0)
            return -1;

        logging(LOG_INFO, "zfactor: ps: id = %d", ps_id);
        zfac_ids[0] = ps_id;
        return 1;
    }
    return 0;
}


#ifdef TEST_MAIN2
int
test_zfactor(void)
{
    {
        int sigid, nids;
        int rval;

        rval = get_axis_ids(&sigid, &nids, "CSIG20", 1, 20, NULL, NULL);

        assert(rval == 0);
        assert(sigid >= 0);
        assert(nids == 1);

        rval = std_sigma(sigid, "CSIG20", 1, 20);
    }
    printf("test_zfactor(): DONE\n");
    return 0;
}
#endif /* TEST_MAIN2 */
