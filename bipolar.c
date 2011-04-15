/*
 * bipolar.c - bipolar mapping used in MIROC5(med).
 *
 * References:
 *   Bentsen et al. 1999, Monthly Weather Review, 127, 2733
 */
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cmor.h"

#include "internal.h"
#include "logging.h"

#define RAD2DEG(x) (180. * (x) / M_PI)
#define DEG2RAD(x) (M_PI * (x) / 180.)

#define A_LONGITUDE 320.
#define A_LATITUDE  80.
#define B_LONGITUDE 320.
#define B_LATITUDE  (-80.)
#define C_LONGITUDE 320.
#define C_LATITUDE  0.


/*
 * polar coordinates on complex plane.
 * z = r * exp(i th)
 *
 * R_INF means that r is infinity.
 */
struct Polar_t {
    double r;                   /* [0 .. +inf] */
    double th;                  /* in radian */
};
typedef struct Polar_t Polar;
#define R_INF  (1e30)           /* +inf */


static double
divmod2(double x, double y)
{
    double r;

    r = floor(x / y);
    return x - r * y;
}


/*
 * lon, lat (in degree) => polar
 */
static Polar
get_polar(double lon, double lat)
{
    Polar z;

    if (lat <= -90.)
        z.r = R_INF;
    else if (lat >= 90.)
        z.r = 0.;
    else
        z.r = tan(0.5 * DEG2RAD(90. - lat));

    z.th = DEG2RAD(lon);
    return z;
}


static void
get_lonlat(double *lon, double *lat, Polar z)
{
    *lat = (z.r == R_INF) ? -90. : 90. - RAD2DEG(2. * atan(z.r));
    *lon = RAD2DEG(z.th);
}


static Polar
psub(Polar a, Polar b)
{
    double x, y;
    Polar z;

    assert(b.r != R_INF);       /* XXX */

    if (a.r == R_INF)
        return a;

    x = a.r * cos(a.th) - b.r * cos(b.th);
    y = a.r * sin(a.th) - b.r * sin(b.th);
    z.r = hypot(x, y);
    z.th = atan2(y, x);
    return z;
}


static Polar
pmul(Polar a, Polar b)
{
    Polar z;

    if (a.r == 0. || b.r == 0.)
        z.r = 0.;
    else if (a.r == R_INF || b.r == R_INF)
        z.r = R_INF;
    else
        z.r = a.r * b.r;

    z.th = a.th + b.th;
    return z;
}


static Polar
reciprocal(Polar a)
{
    Polar z;

    if (a.r == R_INF)
        z.r = 0.;
    else if (a.r == 0)
        z.r = R_INF;
    else
        z.r = 1. / a.r;

    z.th = -a.th;
    return z;
}


static Polar
pdiv(Polar a, Polar b)
{
    return pmul(a, reciprocal(b));
}


/*
 * w = ((z - a) (c - b)) / ((z - b) (c - a))
 * Eq.(6) in Bentsen.
 */
static void
forward_f(Polar *w, const Polar *z, size_t size,
          Polar a, Polar b, Polar c)
{
    Polar ca, cb, u, v;
    int i;

    ca = psub(c, a);
    cb = psub(c, b);
    for (i = 0; i < size; i++) {
        u = pmul(psub(z[i], a), cb);
        v = pmul(psub(z[i], b), ca);

        w[i] = pdiv(u, v);
    }
}


/*
 * z = (-b w (c - a) + a (c - b)) / (-w (c - a) + (c - b))
 * Eq(7) in Bentsen.
 */
static void
backward_f(Polar *z, const Polar *w, size_t size,
           Polar a, Polar b, Polar c)
{
    int i;
    Polar ca, cb, bca, acb;
    Polar u, v;

    ca = psub(c, a);
    cb = psub(c, b);
    bca = pmul(b, ca);
    acb = pmul(a, cb);
    for (i = 0; i < size; i++) {
        if (w[i].r == R_INF) {
            z[i] = b;
            continue;
        }
        u = psub(pmul(w[i], bca), acb);
        v = psub(pmul(w[i], ca), cb);
        z[i] = pdiv(u, v);
    }
}


static int
set_mapping_params(int grid_id)
{
    logging(LOG_INFO, "No mapping name for bipolar.");
    return 0;
}


/*
 * backward transformation (from fake to true).
 *
 * Arguments:
 *   lon, lat: true longitude and latitude.
 *   x, y:     fake longitude and latitude.
 */
static int
backward_transform(double *lon, double *lat,
                   const double *x, const double *y,
                   int xlen, int ylen)
{
    const double MOD_PHASE = M_PI; /* XXX: in MIROC5 */
    Polar a, b, c;
    Polar *w = NULL, *z = NULL;
    int i, j, rval = -1;

    if ((w = malloc(sizeof(Polar) * xlen * ylen)) == NULL
        || (z = malloc(sizeof(Polar) * xlen * ylen)) == NULL) {
        logging(LOG_SYSERR, NULL);
        goto finish;
    }

    for (j = 0; j < ylen; j++)
        for (i = 0; i < xlen; i++) {
            w[i + xlen * j] = get_polar(x[i], y[j]);

            /*
             * NOTE:
             * Mapping function is slightly different from that of Bentsen.
             *
             * In Bentsen: f(a) = 0, f(b) = inf, f(c) = 1.
             * In MIROC5:  f(a) = 0, f(b) = inf, f(c) = -1.
             *
             * MOD_PHASE is needed to get around this difference.
             */
            w[i + xlen * j].th += MOD_PHASE;
        }

    a = get_polar(A_LONGITUDE, A_LATITUDE);
    b = get_polar(B_LONGITUDE, B_LATITUDE);
    c = get_polar(C_LONGITUDE, C_LATITUDE);
    backward_f(z, w, xlen * ylen, a, b, c);

    for (i = 0; i < xlen * ylen; i++) {
        get_lonlat(lon + i, lat + i, z[i]);
        lon[i] = divmod2(lon[i], 360.);
    }
    rval = 0;

finish:
    free(z);
    free(w);
    return rval;
}


/*
 * setup grid mapping: bipolar.
 */
int
setup_bipolar(int *grid_id,
              const double *xx, const double *x_bnds, int x_len,
              const double *yy, const double *y_bnds, int y_len)
{
    int id;
    int axes_ids[2];
    double *lon = NULL, *lat = NULL;
    double *lon_vertices = NULL;
    double *lat_vertices = NULL;
    double *lon_bnds = NULL;
    double *lat_bnds = NULL;
    int rval = -1;
    int len, lenv;
    int i, j, n, vp0, vp1, vp2, vp3;
#if 0
    /* x_deg and y_deg are removed (28 March 2012). */
    const char *xname = "x_deg";
    const char *yname = "y_deg";
#else
    const char *xname = "grid_longitude";
    const char *yname = "grid_latitude";
#endif

    /*
     * (not true) latitude and longitude.
     */
    if (cmor_axis(&axes_ids[0],
                  (char *)yname, "degrees", y_len,
                  (double *)yy, 'd', (double *)y_bnds, 1, NULL) != 0
        || cmor_axis(&axes_ids[1],
                     (char *)xname, "degrees", x_len,
                     (double *)xx, 'd', (double *)x_bnds, 1, NULL) != 0) {
        logging(LOG_ERR, "cmor_axis() before cmor_grid() failed.");
        return -1;
    }
    logging(LOG_INFO, "%s id = %d", yname, axes_ids[0]);
    logging(LOG_INFO, "%s id = %d", xname, axes_ids[1]);

    /*
     * lat(yy, xx) and lat(yy, xx).
     */
    len = x_len * y_len;
    lenv = (x_len + 1) * (y_len + 1);
    if ((lon = malloc(sizeof(double) * len)) == NULL
        || (lat = malloc(sizeof(double) * len)) == NULL
        || (lon_bnds = malloc(sizeof(double) * lenv)) == NULL
        || (lat_bnds = malloc(sizeof(double) * lenv)) == NULL
        || (lon_vertices = malloc(sizeof(double) * 4 * len)) == NULL
        || (lat_vertices = malloc(sizeof(double) * 4 * len)) == NULL) {
        logging(LOG_SYSERR, NULL);
        goto finish;
    }

    /*
     * coordinates transformation (from fake to true).
     */
    if (backward_transform(lon, lat, xx, yy, x_len, y_len) < 0
        || backward_transform(lon_bnds, lat_bnds,
                              x_bnds, y_bnds,
                              x_len + 1, y_len + 1) < 0)
        goto finish;

    /*
     * make vertices.
     */
    for (j = 0; j < y_len; j++)
        for (i = 0; i < x_len; i++) {
            n = i + x_len * j;

            vp0 = n + j;
            vp1 = vp0 + 1;
            vp2 = vp1 + x_len + 1;
            vp3 = vp0 + x_len + 1;

            lon_vertices[4 * n + 0] = lon_bnds[vp0];
            lon_vertices[4 * n + 1] = lon_bnds[vp1];
            lon_vertices[4 * n + 2] = lon_bnds[vp2];
            lon_vertices[4 * n + 3] = lon_bnds[vp3];

            lat_vertices[4 * n + 0] = lat_bnds[vp0];
            lat_vertices[4 * n + 1] = lat_bnds[vp1];
            lat_vertices[4 * n + 2] = lat_bnds[vp2];
            lat_vertices[4 * n + 3] = lat_bnds[vp3];
        }

    if (cmor_grid(&id, 2, axes_ids, 'd',
                  lat, lon,
                  4,
                  lat_vertices,
                  lon_vertices) != 0) {
        logging(LOG_ERR, "cmor_grid() failed.");
        goto finish;
    }
    logging(LOG_INFO, "grid id = %d", id);

    if (set_mapping_params(id) < 0)
        goto finish;

    *grid_id = id;
    rval = 0;

finish:
    free(lat_vertices);
    free(lon_vertices);
    free(lat_bnds);
    free(lon_bnds);
    free(lat);
    free(lon);
    return rval;
}


#ifdef TEST_MAIN2
#include <stdio.h>

#define EPS  1e-13

static int
equal(double x, double ref)
{
    if (x == ref)
        return 255;
    if (fabs(ref) < EPS)
        return fabs(x) < EPS;

    return fabs(1. - x / ref) < EPS;
}


static int
equal_polar(Polar a, Polar b)
{
    if (b.r == R_INF)
        return a.r == R_INF;

    if (equal(a.r, b.r)) {
        a.th -= b.th;
        a.th = divmod2(a.th, 2. * M_PI);

        return equal(a.th, 0.) || equal(a.th, 2. * M_PI);
    }
    return 0;
}


static void
test1(void)
{
    Polar a;
    double dlon, dlat;
    double lon, lat, rlon, rlat;
    int i, j;
    int nlon = 36;
    int nlat = 18;

    dlon = 360. / nlon;
    dlat = 180. / nlat;
    for (j = 1; j < nlat; j++)
        for (i = 0; i < nlon + 1; i++) {
            lon = i * dlon;
            lat = -90. + j * dlat;

            a = get_polar(lon, lat);
            get_lonlat(&rlon, &rlat, a);

            assert(equal(lon, rlon));
            assert(equal(lat, rlat));
        }

    for (i = 0; i < nlon + 1; i++) {
        lon = i * dlon;
        lat = -90.;
        a = get_polar(lon, lat);
        get_lonlat(&rlon, &rlat, a);

        assert(equal(lon, rlon));
        assert(equal(lat, rlat));
    }

    for (i = 0; i < nlon + 1; i++) {
        lon = i * dlon;
        lat = 90.;
        a = get_polar(lon, lat);
        get_lonlat(&rlon, &rlat, a);

        assert(equal(lon, rlon));
        assert(equal(lat, rlat));
    }
}


static void
test2(void)
{
    Polar a, b, c;
    Polar w, z;
    int n;

    a = get_polar(320., 80.);
    b = get_polar(320., -80.);
    c = get_polar(320., 0.);

    /* f(a) = 0 */
    z = a;
    forward_f(&w, &z, 1, a, b, c);
    assert(w.r == 0.);

    /* f(b) = inf */
    z = b;
    forward_f(&w, &z, 1, a, b, c);
    assert(w.r == R_INF);

    /* f(c) = 1 */
    z = c;
    forward_f(&w, &z, 1, a, b, c);
    assert(w.r == 1. && w.th == 0.);

    /* f^{-1}(1) = c */
    z.r = 1.;
    z.th = 0.;
    backward_f(&w, &z, 1, a, b, c);
    assert(equal_polar(w, c));

    /* f^{-1}(0) = a */
    z.r = z.th = 0.;
    backward_f(&w, &z, 1, a, b, c);
    assert(equal_polar(w, a));

    /* f^{-1}(0) = a */
    z.r = 0.;
    for (n = 0; n < 10; n++) {
        z.th = 1. * n;
        backward_f(&w, &z, 1, a, b, c);
        assert(equal_polar(w, a));
    }

    /* f^{-1}(inf) = b */
    z.r = R_INF;
    z.th = 0.;
    backward_f(&w, &z, 1, a, b, c);
    assert(equal_polar(w, b));
}


static void
test3(void)
{
    assert(divmod2(-40., 360.) == 320.);
    assert(divmod2(360., 360.) == 0.);
    assert(divmod2(400., 360.) == 40.);
    assert(divmod2(-400., 360.) == 320.);
}


static void
test4(void)
{
    double x[] = {180.};
    double y[] = {-90., 0., 90};
    double lon[6], lat[6];
    size_t xlen, ylen;
    int rval;

    xlen = 1;
    ylen = 3;
    rval = backward_transform(lon, lat, x, y, xlen, ylen);
    assert(rval == 0);
#if 0
    {
        int i;

        for (i = 0; i < xlen * ylen; i++)
            printf("%20.4f %20.4f\n", lon[i], lat[i]);
    }
#endif
}


static void
test5(void)
{
    Polar z, z2, w, z3;
    Polar a, b, c;
    int nlon = 360;
    int nlat = 179;
    double lon, lat, x, y;
    int i, j;

    a = get_polar(A_LONGITUDE, A_LATITUDE);
    b = get_polar(B_LONGITUDE, B_LATITUDE);
    c = get_polar(C_LONGITUDE, C_LATITUDE);
    for (j = 0; j < nlat; j++) {
        lat = -89. + j;

        for (i = 0; i < nlon; i++) {
            lon = (double)i;

            z = get_polar(lon, lat);
            get_lonlat(&x, &y, z);
            z2 = get_polar(x, y);
            assert(equal_polar(z2, z));

            /*
             * forward and backward.
             */
            forward_f(&w, &z, 1, a, b, c);
            backward_f(&z3, &w, 1, a, b, c);
            assert(equal_polar(z3, z));
        }
    }
}


static void
test6(void)
{
    Polar a, b, c;
    Polar w, z;
    double lon;
    int i, nlon = 361;

    a = get_polar(320., 80.);
    b = get_polar(320., -80.);
    c = get_polar(320., 0.);

    for (i = 0; i < nlon; i++) {
        lon = (double)i;

        /* North pole */
        w = get_polar(lon, 90.);
        backward_f(&z, &w, 1, a, b, c);
        assert(equal_polar(z, a));

        /* South pole */
        w = get_polar(lon, -90.);
        backward_f(&z, &w, 1, a, b, c);
        assert(equal_polar(z, b));
    }
}


int
test_bipolar(void)
{
    test1();
    test2();
    test3();
    test4();
    test5();
    test6();

    printf("test_bipolar(): DONE\n");
    return 0;
}
#endif /* TEST_MAIN2 */
