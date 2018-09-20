/*
 * tripolar.c - tripolar mapping used in MIROC5.2(med52).
 *
 * References:
 *   Bentsen et al. 1999, Monthly Weather Review, 127, 2733
 *   Murray, 1996, Jornal of Computational Physics, 126, 251
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

/*
 * Grid poles for bipolar
 *
 * A: 63S, 60E   (Siberia)
 * B: 63S, 120W  (Canada)
 *
 * C: Mid point between A and B
 */
#define POLE_LATITUDE 63.33370028342989

#define A_LONGITUDE 60.
#define A_LATITUDE  POLE_LATITUDE
#define B_LONGITUDE 240.
#define B_LATITUDE  POLE_LATITUDE
#define C_LONGITUDE 150.
#define C_LATITUDE  90.


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

    if (a.r == b.r && a.th == b.th) {
        z.r = 0.;
        z.th = 0.;
        return z;
    }
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
    Polar a, b, c;
    Polar *w = NULL, *z = NULL;
    double rlon, rlat;
    int i, j, ij, rval = -1;
    int joint_index = 0;
    const double EPS = 1e-7;

    if ((w = malloc(sizeof(Polar) * xlen)) == NULL
        || (z = malloc(sizeof(Polar) * xlen)) == NULL) {
        logging(LOG_SYSERR, NULL);
        goto finish;
    }

    for (j = 0; j < ylen; j++)
        if (y[j] >= POLE_LATITUDE - EPS) {
            joint_index = j;
            break;
        }

    /*
     * south (lat/lon grid)
     */
    for (j = 0; j < joint_index; j++) {
        for (i = 0; i < xlen; i++) {
            ij = i + xlen * j;
            lon[ij] = divmod2(A_LONGITUDE + x[i], 360.);
            lat[ij] = y[j];
        }
    }

    /*
     * north (bipolar grid)
     */
    a = get_polar(A_LONGITUDE, A_LATITUDE);
    b = get_polar(B_LONGITUDE, B_LATITUDE);
    c = get_polar(C_LONGITUDE, C_LATITUDE);
    for (j = joint_index; j < ylen; j++) {
        for (i = 0; i < xlen; i++) {
            /*
             * (xdeg, ydeg) -> (rlon, rlat)
             */
            if (x[i] <= 180.) {
                rlat = 90. - x[i];
                rlon = -90. + (y[j] - POLE_LATITUDE);
            } else {
                rlat = x[i] - 270.;
                rlon = 90. - (y[j] - POLE_LATITUDE);
            }
            w[i] = get_polar(rlon, rlat);
        }
        backward_f(z, w, xlen, a, b, c);

        for (i = 0; i < xlen; i++) {
            ij = i + xlen * j;

            get_lonlat(lon + ij, lat + ij, z[i]);
            lon[ij] = divmod2(lon[ij], 360.);
        }
    }
    rval = 0;

finish:
    free(z);
    free(w);
    return rval;
}


/*
 * setup grid mapping: tripolar.
 */
int
setup_tripolar(int *grid_id,
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
    const char *xname = "x_deg";
    const char *yname = "y_deg";

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


/*
 * (rlon, rlat) -> (lon, lat)
 */
static void
bipolar(double *lon, double *lat, double rlon, double rlat,
        Polar a, Polar b, Polar c)
{
    Polar z, w;

    w = get_polar(rlon, rlat);
    backward_f(&z, &w, 1, a, b, c);

    get_lonlat(lon, lat, z);
    *lon = divmod2(*lon, 360.);
}


/*
 * (xdeg,ydeg) -> (lon, lat)
 */
static void
transpose(double *lon, double *lat, double xdeg, double ydeg,
          Polar a, Polar b, Polar c)
{
    double rlon, rlat;

    /*
     * ydeg -> rlon
     * xdeg -> rlat
     */
    if (xdeg <= 180.) {
        rlat = 90. - xdeg;
        rlon = -90. + (ydeg - POLE_LATITUDE);
    } else {
        rlat = xdeg - 270.;
        rlon = 90. - (ydeg - POLE_LATITUDE);
    }
    bipolar(lon, lat, rlon, rlat, a, b, c);
}


static void
test_bipolar(double rlon, double rlat)
{
    Polar a, b, c;
    double lon, lat;

    a = get_polar(A_LONGITUDE, A_LATITUDE);
    b = get_polar(B_LONGITUDE, B_LATITUDE);
    c = get_polar(C_LONGITUDE, C_LATITUDE);
    bipolar(&lon, &lat, rlon, rlat, a, b, c);
    printf("bipolar: (%12.5f,%12.5f) -> (%12.5f,%12.5f)\n",
           rlon, rlat, lon, lat);
}


static void
test_transpose(double xdeg, double ydeg)
{
    Polar a, b, c;
    double lon, lat;

    a = get_polar(A_LONGITUDE, A_LATITUDE);
    b = get_polar(B_LONGITUDE, B_LATITUDE);
    c = get_polar(C_LONGITUDE, C_LATITUDE);
    transpose(&lon, &lat, xdeg, ydeg, a, b, c);
    printf("transpose: (%12.5f,%12.5f) -> (%12.5f,%12.5f)\n",
           xdeg, ydeg, lon, lat);
}


static void
test1()
{
    /* test (rlon,rlat) -> (lon, lat) */
    test_bipolar(0, 0.001);
    test_bipolar(-0.001, 0.);
    test_bipolar(0, -0.001);
    test_bipolar(0.001, 0.);
    test_bipolar(0, 0.002);
    test_bipolar(-0.002, 0.);
    test_bipolar(0, -0.002);
    test_bipolar(0.002, 0.);

    test_bipolar(-90., 90);
    test_bipolar(-90., 0.);
    test_bipolar(-90., -90);
    test_bipolar(90., 0.);

    /* test (xdeg,ydeg) -> (lon, lat) */
    test_transpose(90., POLE_LATITUDE);

    test_transpose(0., POLE_LATITUDE + 89.);
    test_transpose(90., POLE_LATITUDE + 89.);
    test_transpose(180., POLE_LATITUDE + 89.);
    test_transpose(270., POLE_LATITUDE + 89.);
}


int
test_tripolar(void)
{
    test1();
    printf("test_tripolar(): DONE\n");
    return 0;
}
#endif
