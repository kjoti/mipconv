/*
 * zfactor.c
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "internal.h"



/*
 * call cmor_zfactor() for CSIG*.
 *
 * set parameter, "ptop", "sigma".
 * p(n,k,j,i) = ptop + sigma(k)*(ps(n,j,i) - ptop)
 */
static int
std0_sigma(int sigid, const char *aitm, int astr, int aend)
{
    int rval = 0;
    GT3_Dim *dim;
    double p0 = 0.;
    int p0_id, b_id;

    if ((dim = GT3_getDim(aitm)) == NULL) {
        GT3_printErrorMessages(stderr);
        return -1;
    }

    /* ptop */
    rval = cmor_zfactor(&p0_id, sigid, "ptop", "Pa",
                        0, NULL, 'd', &p0, NULL);
    
    /* sigma */
    rval = cmor_zfactor(&b_id, sigid, "sigma", " ",
                        1, &sigid, 'd', 
                        dim->values + astr - 1, 
                        NULL);

    logging(LOG_INFO, "zfactor: ptop:  id = %d", p0_id);
    logging(LOG_INFO, "zfactor: sigma: id = %d", b_id);
    assert(p0_id >= 0);
    assert(b_id >= 0);

    GT3_freeDim(dim);
    return rval;
}


/*
 * call cmor_zfactor() for CSIG*.
 *
 * for formula: p(n,k,j,i) = a(k)*p0 + b(k)*ps(n,j,i)
 */
static int
std_sigma(int sigid, const char *aitm, int astr, int aend)
{
    int rval = 0;
    char name[17];
    GT3_Dim *dim, *dimb;
    double p0 = 0.;
    double *coef; /* a or b */
    int dimlen = aend - astr + 1;
    int i, p0_id, a_id, b_id;

    snprintf(name, sizeof name, "%s.M", aitm);
    if ((dim = GT3_getDim(aitm)) == NULL
        || (dimb = GT3_getDim(name)) == NULL) {
        GT3_printErrorMessages(stderr);
        return -1;
    }

    if ((coef = malloc(sizeof(double) * dimlen)) == NULL) {
        rval = -1;
        goto finish;
    }
    for (i = 0; i < dimlen; i++)
        coef[i] = 0.;

    /* p0 */
    rval = cmor_zfactor(&p0_id, sigid, "p0", "Pa",
                        0, NULL, 'd', &p0, NULL);
    
    /* a and a_bnds */
    rval = cmor_zfactor(&a_id, sigid, "a", " ",
                        1, &sigid, 'd', coef, coef);

    /* b and b_bnds */
    rval = cmor_zfactor(&b_id, sigid, "b", " ",
                        1, &sigid, 'd', 
                        dim->values + astr - 1, 
                        dimb->values + astr - 1);

    logging(LOG_INFO, "zfactor: p0: id = %d", p0_id);
    logging(LOG_INFO, "zfactor: a:  id = %d", a_id);
    logging(LOG_INFO, "zfactor: b:  id = %d", b_id);

#if 0
    /* a_bnd */
    rval = cmor_zfactor(&a_id, sigid, "a_bnds", " ",
                        1, &sigid, 'd', coef, NULL);

    /* b_bnds */
    rval = cmor_zfactor(&b_id, sigid, "b_bnds", " ",
                        1, &sigid, 'd', 
                        dimb->values + astr, 
                        NULL);

    logging(LOG_INFO, "zfactor: a_bnds:  id = %d", a_id);
    logging(LOG_INFO, "zfactor: b_bnds:  id = %d", b_id);
#endif
    assert(p0_id >= 0);
    assert(a_id >= 0);
    assert(b_id >= 0);
finish:
    free(coef);
    GT3_freeDim(dimb);
    GT3_freeDim(dim);
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

        if (std_sigma(sig_id, aitm, astr, aend) < 0)
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
