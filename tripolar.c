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
#define DEFAULT_POLE_LATITUDE 63.

static double pole_latitude = DEFAULT_POLE_LATITUDE;
static double a_longitude = 60.;
static double a_latitude = DEFAULT_POLE_LATITUDE;
static double b_longitude = 240.;
static double b_latitude = DEFAULT_POLE_LATITUDE;
static double c_longitude = 150.;
static double c_latitude = 90.;


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
    else if (a.r == 0.)
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
    double xdeg, rlon, rlat;
    int i, j, ij, rval = -1;
    int joint_index = 0;
    const double EPS = 1e-7;

    if ((w = malloc(sizeof(Polar) * xlen)) == NULL
        || (z = malloc(sizeof(Polar) * xlen)) == NULL) {
        logging(LOG_SYSERR, NULL);
        goto finish;
    }

    for (j = 0; j < ylen; j++)
        if (y[j] >= pole_latitude - EPS) {
            joint_index = j;
            break;
        }

    /*
     * south (lat/lon grid)
     */
    for (j = 0; j < joint_index; j++) {
        for (i = 0; i < xlen; i++) {
            ij = i + xlen * j;
            lon[ij] = divmod2(a_longitude + x[i], 360.);
            lat[ij] = y[j];
        }
    }

    /*
     * north (bipolar grid)
     */
    a = get_polar(a_longitude, a_latitude);
    b = get_polar(b_longitude, b_latitude);
    c = get_polar(c_longitude, c_latitude);
    for (j = joint_index; j < ylen; j++) {
        for (i = 0; i < xlen; i++) {
            /*
             * (xdeg, ydeg) -> (rlon, rlat)
             */
            xdeg = divmod2(x[i], 360.);
            if (xdeg <= 180.) {
                rlat = 90. - xdeg;
                rlon = -90. + (y[j] - pole_latitude);
            } else {
                rlat = xdeg - 270.;
                rlon = 90. - (y[j] - pole_latitude);
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


static void
set_pole_position(double plat)
{
    pole_latitude = a_latitude = b_latitude = plat;
}


/*
 * setup grid mapping: tripolar.
 */
int
setup_tripolar(int *grid_id,
               const double *xx, const double *x_bnds, int x_len,
               const double *yy, const double *y_bnds, int y_len,
               double plat)
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

    set_pole_position(plat);

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
    xdeg = divmod2(xdeg, 360.);
    if (xdeg <= 180.) {
        rlat = 90. - xdeg;
        rlon = -90. + (ydeg - pole_latitude);
    } else {
        rlat = xdeg - 270.;
        rlon = 90. - (ydeg - pole_latitude);
    }
    bipolar(lon, lat, rlon, rlat, a, b, c);
}


static void
test_bipolar()
{
    Polar a, b, c;
    double lon, lat, rlon, rlat;
    int i;

    a = get_polar(a_longitude, a_latitude);
    b = get_polar(b_longitude, b_latitude);
    c = get_polar(c_longitude, c_latitude);

    /* North pole */
    for (i = -90; i <= 90; i++) {
        rlon = (double)i;
        rlat = 90.;
        bipolar(&lon, &lat, rlon, rlat, a, b, c);
        assert(equal(lon, a_longitude));
        assert(equal(lat, a_latitude));
    }

    /* South pole */
    for (i = -90; i <= 90; i++) {
        rlon = (double)i;
        rlat = -90.;
        bipolar(&lon, &lat, rlon, rlat, a, b, c);
        assert(equal(lon, b_longitude));
        assert(equal(lat, b_latitude));
    }

    /* Meridian (north) */
    for (i = 1; i <= 90; i++) {
        rlon = 0.;
        rlat = (double)i;
        bipolar(&lon, &lat, rlon, rlat, a, b, c);
        assert(equal(lon, a_longitude));
    }

    /* Meridian (south) */
    for (i = -90; i < 0; i++) {
        rlon = 0.;
        rlat = (double)i;
        bipolar(&lon, &lat, rlon, rlat, a, b, c);
        assert(equal(lon, b_longitude));
    }

    /* Equator */
    bipolar(&lon, &lat, -90., 0., a, b, c);
    assert(equal(lon, a_longitude + 90.));
    assert(equal(lat, pole_latitude));
}


static void
test_transpose()
{
    Polar a, b, c;
    double xdeg, ydeg, lon, lat;
    int i;

    a = get_polar(a_longitude, a_latitude);
    b = get_polar(b_longitude, b_latitude);
    c = get_polar(c_longitude, c_latitude);

    /* Siberia (pole 1) */
    for (i = 0; i <= 90; i++) {
        xdeg = 0.;
        ydeg = pole_latitude + i;
        transpose(&lon, &lat, xdeg, ydeg, a, b, c);

        assert(equal(lon, a_longitude));
        assert(equal(lat, a_latitude));
    }

    /* Canada (pole 2) */
    for (i = 0; i <= 90; i++) {
        xdeg = 180.;
        ydeg = pole_latitude + i;
        transpose(&lon, &lat, xdeg, ydeg, a, b, c);

        assert(equal(lon, b_longitude));
        assert(equal(lat, b_latitude));
    }

    /* Joint latitude */
    for (i = 0; i < 720; i++) {
        xdeg = (double)i;
        ydeg = pole_latitude;
        transpose(&lon, &lat, xdeg, ydeg, a, b, c);
        lon = divmod2(lon, 360.);

#if 0
        printf("tripolar: (%12.5f,%12.5f) -> (%12.5f,%12.5f)\n",
               xdeg, ydeg, lon, lat);
#endif
        assert(equal(lon, divmod2(xdeg + a_longitude, 360.)));
        assert(equal(lat, pole_latitude));
    }

    /* North pole */
    for (i = 0; i <= 1; i++) {
        xdeg = 90. + 180 * i;
        ydeg = pole_latitude + 90.;
        transpose(&lon, &lat, xdeg, ydeg, a, b, c);

#if 0
        printf("tripolar: (%12.5f,%12.5f) -> (%12.5f,%12.5f)\n",
               xdeg, ydeg, lon, lat);
#endif
        assert(equal(lat, 90.));
    }
}


int
test_tripolar(void)
{
    set_pole_position(63.3337);
    test_bipolar();
    test_transpose();

    set_pole_position(63.0);
    test_bipolar();
    test_transpose();
    printf("test_tripolar(): DONE\n");
    return 0;
}
#endif
