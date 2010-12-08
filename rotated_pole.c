/*
 * rotated_pole.c -- set up rotated pole mapping.
 *
 * grid_mapping_name: rotated_latitude_longitude
 *
 * Map parameters:
 *   + grid_north_pole_latitude
 *      True latitude (degrees_north) of the north pole of the rotated grid.
 *
 *   + grid_north_pole_longitude
 *      True longitude (degrees_east) of the north pole of the rotated grid.
 *
 *   + north_pole_grid_longitude
 *      Longitude (degrees) of the true north pole in the rotated grid.
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "cmor.h"

#include "internal.h"
#include "myutils.h"
#include "logging.h"

#if 1
#define NORTH_POLE_LAT 77.
#define NORTH_POLE_LON -40.
#define LONGITUDE_OF_TRUE_NP 90.
#else
#define NORTH_POLE_LAT 90.
#define NORTH_POLE_LON 0.
#define LONGITUDE_OF_TRUE_NP 180.
#endif


/*
 * set mapping parameters for "rotated pole".
 */
static int
set_mapping_params(int grid_id)
{
    const char *mapping_name = "rotated_latitude_longitude";
    char *names_[] = {
        "grid_north_pole_latitude",
        "grid_north_pole_longitude",
        "north_pole_grid_longitude"
    };
    char *units_[] = { "degrees_north", "degrees_east", "degrees_east" };
    double values[] = {
        NORTH_POLE_LAT,
        NORTH_POLE_LON,
        LONGITUDE_OF_TRUE_NP
    };
    int rval;
    /*
     * XXX: CMOR2 implementation requires string array which has
     * the uniform length, such as LEN_OF_NAME and LEN_OF_UNIT.
     */
#define LEN_OF_NAME 32
#define LEN_OF_UNIT 16
#define NUM_PARAMS 3
    char names[NUM_PARAMS][LEN_OF_NAME];
    char units[NUM_PARAMS][LEN_OF_UNIT];
    int i;

    assert(sizeof names_ / sizeof names_[0] == NUM_PARAMS);
    assert(sizeof units_ / sizeof units_[0] == NUM_PARAMS);
    for (i = 0; i < NUM_PARAMS; i++) {
        strlcpy(names[i], names_[i], LEN_OF_NAME);
        strlcpy(units[i], units_[i], LEN_OF_UNIT);
    }
    rval = cmor_set_grid_mapping(grid_id,
                                 (char *)mapping_name,
                                 NUM_PARAMS,
                                 (char **)names,
                                 LEN_OF_NAME,
                                 values,
                                 (char **)units,
                                 LEN_OF_UNIT);

    return rval != 0 ? -1 : 0;
}


/*
 * setup grid mapping: rotated pole.
 */
int
setup_rotated_pole(int *grid_id,
                   const double *rlon, const double *rlon_bnds, int rlonlen,
                   const double *rlat, const double *rlat_bnds, int rlatlen)
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

    /*
     * rlat and rlon.
     * rotated (not true) latitude and longitude.
     */
    if (   cmor_axis(&axes_ids[0],
                     "grid_latitude", "degrees_north", rlatlen,
                     (double *)rlat, 'd', (double *)rlat_bnds, 1, NULL) != 0
        || cmor_axis(&axes_ids[1],
                     "grid_longitude", "degrees_east", rlonlen,
                     (double *)rlon, 'd', (double *)rlon_bnds, 1, NULL) != 0) {

        logging(LOG_ERR, "cmor_axis() before cmor_grid() failed.");
        return -1;
    }
    logging(LOG_INFO, "rlat id = %d", axes_ids[0]);
    logging(LOG_INFO, "rlon id = %d", axes_ids[1]);

    /*
     * lat(rlat, rlon) and lat(rlat, rlon).
     */
    len = rlonlen * rlatlen;
    lenv = (rlonlen + 1) * (rlatlen + 1);
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
     * coordinates translation.
     */
    rotate_lonlat(lon, lat,
                  rlon, rlat, rlonlen, rlatlen,
                  NORTH_POLE_LON,
                  90. - NORTH_POLE_LAT,
                  180. - LONGITUDE_OF_TRUE_NP);
    rotate_lonlat(lon_bnds, lat_bnds,
                  rlon_bnds, rlat_bnds, rlonlen + 1, rlatlen + 1,
                  NORTH_POLE_LON,
                  90. - NORTH_POLE_LAT,
                  180. - LONGITUDE_OF_TRUE_NP);

    for (j = 0; j < rlatlen; j++)
        for (i = 0; i < rlonlen; i++) {
            n = i + rlonlen * j;

            vp0 = n + j;
            vp1 = vp0 + 1;
            vp2 = vp1 + rlonlen + 1;
            vp3 = vp0 + rlonlen + 1;

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
void
test_rotated_pole(void)
{
    int grid_id = 0;

    double lat_bnds[] = { -80., -40., 0., 40., 80. };
    double lon_bnds[] = { 0., 90., 180., 270., 360. };
    double lat[4], lon[4];
    int i;

    if (load_grid_table("Tables/CMIP5_grids") < 0)
        exit(1);

    /*
     * grid.
     */
    for (i = 0; i < 4; i++) {
        lat[i] = .5 * (lat_bnds[i] + lat_bnds[i+1]);
        lon[i] = .5 * (lon_bnds[i] + lon_bnds[i+1]);
    }
    switch_to_grid_table();
    if (setup_rotated_pole(&grid_id,
                           lon, lon_bnds, 4,
                           lat, lat_bnds, 4) < 0) {

        assert(!"failed");
    }

    printf("test_rotated_pole(): DONE\n");
}
#endif /* TEST_MAIN2 */
