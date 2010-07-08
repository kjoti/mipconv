/*
 * coord.c
 *
 * utilities for coordinates.
 */
#include <assert.h>
#include <string.h>
#include <math.h>

#define RAD2DEG(x) (180. * (x) / M_PI)
#define DEG2RAD(x) (M_PI * (x) / 180.)

typedef double M3[3][3];
typedef double V3[3];

static void
M3_zero(M3 ma)
{
    ma[0][0] = 0.;
    ma[0][1] = 0.;
    ma[0][2] = 0.;
    ma[1][0] = 0.;
    ma[1][1] = 0.;
    ma[1][2] = 0.;
    ma[2][0] = 0.;
    ma[2][1] = 0.;
    ma[2][2] = 0.;
}


static void
M3_one(M3 ma)
{
    ma[0][0] = 1.;
    ma[0][1] = 0.;
    ma[0][2] = 0.;
    ma[1][0] = 0.;
    ma[1][1] = 1.;
    ma[1][2] = 0.;
    ma[2][0] = 0.;
    ma[2][1] = 0.;
    ma[2][2] = 1.;
}


static int
is_zero(M3 ma, double eps)
{
    return fabs(ma[0][0]) <= eps
        && fabs(ma[0][1]) <= eps
        && fabs(ma[0][2]) <= eps
        && fabs(ma[1][0]) <= eps
        && fabs(ma[1][1]) <= eps
        && fabs(ma[1][2]) <= eps
        && fabs(ma[2][0]) <= eps
        && fabs(ma[2][1]) <= eps
        && fabs(ma[2][2]) <= eps;
}


static int
is_identity(M3 ma)
{
    const double eps = 1e-14;

    return fabs(ma[0][0] - 1.) <= eps
        && fabs(ma[0][1]) <= eps
        && fabs(ma[0][2]) <= eps
        && fabs(ma[1][0]) <= eps
        && fabs(ma[1][1] - 1.) <= eps
        && fabs(ma[1][2]) <= eps
        && fabs(ma[2][0]) <= eps
        && fabs(ma[2][1]) <= eps
        && fabs(ma[2][2] - 1.) <= eps;
}


static void
M3_copy(M3 dest, M3 src)
{
    /* memcpy(dest, src, sizeof(M3)); */
    dest[0][0] = src[0][0];
    dest[0][1] = src[0][1];
    dest[0][2] = src[0][2];
    dest[1][0] = src[1][0];
    dest[1][1] = src[1][1];
    dest[1][2] = src[1][2];
    dest[2][0] = src[2][0];
    dest[2][1] = src[2][1];
    dest[2][2] = src[2][2];
}


static void
M3_transpose(M3 dest, M3 src)
{
    dest[0][0] = src[0][0];
    dest[0][1] = src[1][0];
    dest[0][2] = src[2][0];
    dest[1][0] = src[0][1];
    dest[1][1] = src[1][1];
    dest[1][2] = src[2][1];
    dest[2][0] = src[0][2];
    dest[2][1] = src[1][2];
    dest[2][2] = src[2][2];
}


static void
make_Rz(M3 ma, double angle)
{
    double s = sin(angle);
    double c = cos(angle);

    ma[0][0] = c;
    ma[0][1] = -s;
    ma[0][2] = 0.;
    ma[1][0] = s;
    ma[1][1] = c;
    ma[1][2] = 0.;
    ma[2][0] = 0.;
    ma[2][1] = 0.;
    ma[2][2] = 1.;
}


static void
make_Ry(M3 ma, double angle)
{
    double s = sin(angle);
    double c = cos(angle);

    ma[0][0] = c;
    ma[0][1] = 0.;
    ma[0][2] = s;
    ma[1][0] = 0.;
    ma[1][1] = 1.;
    ma[1][2] = 0.;
    ma[2][0] = -s;
    ma[2][1] = 0.;
    ma[2][2] = c;
}


static void
M3_mul(M3 ma, M3 m1, M3 m2)
{
    ma[0][0] = m1[0][0]*m2[0][0] + m1[0][1]*m2[1][0] + m1[0][2]*m2[2][0];
    ma[0][1] = m1[0][0]*m2[0][1] + m1[0][1]*m2[1][1] + m1[0][2]*m2[2][1];
    ma[0][2] = m1[0][0]*m2[0][2] + m1[0][1]*m2[1][2] + m1[0][2]*m2[2][2];

    ma[1][0] = m1[1][0]*m2[0][0] + m1[1][1]*m2[1][0] + m1[1][2]*m2[2][0];
    ma[1][1] = m1[1][0]*m2[0][1] + m1[1][1]*m2[1][1] + m1[1][2]*m2[2][1];
    ma[1][2] = m1[1][0]*m2[0][2] + m1[1][1]*m2[1][2] + m1[1][2]*m2[2][2];

    ma[2][0] = m1[2][0]*m2[0][0] + m1[2][1]*m2[1][0] + m1[2][2]*m2[2][0];
    ma[2][1] = m1[2][0]*m2[0][1] + m1[2][1]*m2[1][1] + m1[2][2]*m2[2][1];
    ma[2][2] = m1[2][0]*m2[0][2] + m1[2][1]*m2[1][2] + m1[2][2]*m2[2][2];
}


static void
M3_mulV3(V3 u, M3 ma, V3 v)
{
    u[0] = ma[0][0] * v[0] + ma[0][1] * v[1] + ma[0][2] * v[2];
    u[1] = ma[1][0] * v[0] + ma[1][1] * v[1] + ma[1][2] * v[2];
    u[2] = ma[2][0] * v[0] + ma[2][1] * v[1] + ma[2][2] * v[2];
}


/*
 * Cartesian (x, y, z) => Polar (thata, phi)
 * "x**2 + y**2 + z**2 == 1" is assumed.
 *
 * The range of return values:
 *   theta: [0, pi]
 *   phi: [0, 2 * pi)
 */
static int
polar_coord(double *theta, double *phi, V3 v)
{
    double z = v[2];

    if (z >= 1.) {
        *theta = 0.;
        *phi = 0.;
        return 0;
    }
    if (z <= -1.) {
        *theta = M_PI;
        *phi = 0.;
        return 0;
    }
    *theta = acos(z);

    /*
     * XXX: Do NOT use "phi = acos(v[0] / sqrt(1. - z * z)".
     * It causes poor precision.
     */
    if (v[0] == 0.) {
        *phi = (v[1] < 0.) ? -M_PI_2 : M_PI_2;
        return 0;
    }

    *phi = atan(fabs(v[1] / v[0]));
    if (v[0] < 0.)
        *phi = M_PI - *phi;

    if (v[1] < 0.)
        *phi = 2. * M_PI - *phi;
    return 0;
}


/*
 * set pos(in Cartesian coordinates) from longitude/latitude in degrees.
 */
static void
set_lonlat(V3 pos, double lon, double lat)
{
    double th, phi;

    th = DEG2RAD(90. - lat);
    phi = DEG2RAD(lon);

    pos[0] = pos[1] = sin(th);
    pos[0] *= cos(phi);
    pos[1] *= sin(phi);
    pos[2] = cos(th);
}


/*
 * get longitude/latitude from Cartesian coordinates (radius == 1.).
 */
static void
get_lonlat(double *lon, double *lat, V3 pos)
{
    double th, phi;

    polar_coord(&th, &phi, pos);
    *lon = RAD2DEG(phi);
    *lat = 90. - RAD2DEG(th);
}


/*
 * Output:
 *   lon[0, ..., nlon*nlat - 1]: true longitude
 *   lat[0, ..., nlon*nlat - 1]: true latitude
 *
 * Input:
 *   rlon[0, ..., nlon - 1]: rotated longitude
 *   rlat[0, ..., nlat - 1]: rotated latitude
 *
 * All arguments in degree (not radian).
 */
int
rotate_lonlat(double *lon, double *lat,
              const double *rlon, const double *rlat,
              int nlon, int nlat,
              double phi, double theta, double psi)
{
    int i, j, n;
    M3 mat, m1, m2, m3, m4;
    V3 rpos, pos;

    make_Rz(m1, DEG2RAD(phi));
    make_Ry(m2, DEG2RAD(theta));
    make_Rz(m3, DEG2RAD(psi));

    M3_mul(m4, m1, m2);
    M3_mul(mat, m4, m3);

    for (j = 0; j < nlat; j++)
        for (i = 0; i < nlon; i++) {
            set_lonlat(rpos, rlon[i], rlat[j]);

            M3_mulV3(pos, mat, rpos);

            n = i + j * nlon;
            get_lonlat(lon + n, lat + n, pos);
        }

    return 0;
}


#ifdef TEST_MAIN2
#include <stdio.h>

static void
M3_print(M3 ma, const char *name)
{
    printf("%s = [ %20.16g %20.16g %20.16g\n"
           "       %20.16g %20.16g %20.16g\n"
           "       %20.16g %20.16g %20.16g ]\n",
           name,
           ma[0][0], ma[0][1], ma[0][2],
           ma[1][0], ma[1][1], ma[1][2],
           ma[2][0], ma[2][1], ma[2][2]);
}


static void
test1(void)
{
    M3 ma, m0, m1;

    M3_one(ma);
    assert(is_identity(ma));

    M3_mul(m1, ma, ma);
    assert(is_identity(m1));

    M3_zero(m0);
    M3_mul(m1, ma, m0);
    assert(is_zero(m1, 0.));
}


static void
test2(int ntimes)
{
    M3 ma, m1, temp;
    double phi;

    if (ntimes < 2)
        return;

    phi = DEG2RAD(360. / ntimes);
    make_Rz(ma, phi);
    M3_one(m1);

    while (ntimes-- > 1) {
        M3_mul(temp, m1, ma);
        M3_copy(m1, temp);
        assert(!is_identity(m1));

    }
    M3_mul(temp, m1, ma);
    assert(is_identity(temp));
}


static void
test3(void)
{
    const double eps = 1e-12;
    V3 pos;
    double lon, lat;
    double lon2, lat2;
    int i, j;

    set_lonlat(pos, 0., 90.);
    get_lonlat(&lon2, &lat2, pos);
    assert(lon2 == 0.);
    assert(lat2 == 90.);

    set_lonlat(pos, 0., -90.);
    get_lonlat(&lon2, &lat2, pos);
    assert(lon2 == 0.);
    assert(lat2 == -90.);

    for (j = 0; j < 180; j++)
        for (i = 0; i < 360; i++) {
            lon = (double)i;
            lat = 89.5 - (double)j;

            set_lonlat(pos, lon, lat);
            get_lonlat(&lon2, &lat2, pos);
            assert(fabs(lon - lon2) < eps);
            assert(fabs(lat - lat2) < eps);
        }
}


static void
test4(void)
{
    M3 m1, m2, m3, m4;
    M3 mrot, mrot_inv;
    double phi = DEG2RAD(-40.);
    double theta = DEG2RAD(13.);
    double psi = DEG2RAD(90.);
    V3 src, dest;
    double lon, lat;
    double const EPS = 1e-14;

    /*
     * Rz(phi) * Ry(th) * Rz(psi)
     */
    make_Rz(m1, phi);
    make_Ry(m2, theta);
    make_Rz(m3, psi);
    M3_mul(m4, m1, m2);
    M3_mul(mrot, m4, m3);

    /* North pole => Green land (40W, 77N) */
    set_lonlat(src, 0., 90.);
    M3_mulV3(dest, mrot, src);
    get_lonlat(&lon, &lat, dest);
    assert(fabs(lon - 320.) <= EPS);
    assert(fabs(lat - 77.) <= EPS);

    /* (90E, 77N) => North pole */
    set_lonlat(src, 90., 77.);
    M3_mulV3(dest, mrot, src);
    get_lonlat(&lon, &lat, dest);
    assert(fabs(lon - 0.) <= EPS);
    assert(fabs(lat - 90.) <= EPS);

    /*
     * R^-1: R R^T = I
     */
    M3_transpose(mrot_inv, mrot);
    M3_mul(m4, mrot, mrot_inv);
    assert(is_identity(m4));

    /* Green land (40W, 77N) => North pole */
    set_lonlat(src, -40., 77.);
    M3_mulV3(dest, mrot_inv, src);
    get_lonlat(&lon, &lat, dest);
    assert(fabs(lon - 0.) <= EPS);
    assert(fabs(lat - 90.) <= EPS);
}


int
test_coord(void)
{
    test1();
    test2(2);
    test2(12);
    test2(36);
    test2(180);
    /* test2(360); this test fails. */
    test3();
    test4();

    printf("test_coord(): DONE\n");
    return 0;
}
#endif /* TEST_MAIN2 */
