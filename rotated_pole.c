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

#include "myutils.h"
#include "logging.h"


/*
 * setup mapping of "Rotated Pole".
 */
static int
setup_mapping(int grid_id)
{
    const char *mapping_name = "rotated_latitude_longitude";
    char *names_[] = {
        "grid_north_pole_latitude",
        "grid_north_pole_longitude",
        "north_pole_grid_longitude" 
    };
    char *units_[] = { "degrees_north", "degrees_east", "degrees_east" };
    double values[] = {
        80.,
        -50.,
        0.,
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

    return 0;
}


/*
 * XXX: grid_id returned by cmor_grid() is a negative number even if
 * successful end. So it cannot be used as a return value.
 */
static int
setup_grid(int *grid_id,
           const double *lon, const double *lon_bnds,
           const double *lat, const double *lat_bnds,
           int len)
{
    int id;
    int axes_ids[2];

    if (   cmor_axis(&axes_ids[0], 
                     "grid_latitude", "degrees_north", len,
                     (double *)lat, 'd', (double *)lat_bnds, 1, NULL) != 0
        || cmor_axis(&axes_ids[1], 
                     "grid_longitude", "degrees_east", len,
                     (double *)lon, 'd', (double *)lon_bnds, 1, NULL) != 0) {

        logging(LOG_ERR, "cmor_axis() before cmor_grid() failed");
        return -1;
    }

    logging(LOG_INFO, "lat id = %d", axes_ids[0]);
    logging(LOG_INFO, "lon id = %d", axes_ids[1]);

    if (cmor_grid(&id, 2, axes_ids, 'd', 
                  (double *)lat, (double *)lon,
                  2, (double *)lat_bnds, (double *)lon_bnds) != 0) {

        logging(LOG_ERR, "cmor_grid() failed");
        return -1;
    }
    logging(LOG_INFO, "grid id = %d", id);
    *grid_id = id;
    return 0;
}


#ifdef TEST_MAIN2
void
test_rotated_pole(void)
{
    int grid_id = 0;
    int table_id;

    double lat_bnds[] = { -90., -45., 0., 45., 90 };
    double lon_bnds[] = { 0., 90., 180., 270., 360. };
    double lat[4], lon[4];
    int i;

    for (i = 0; i < 4; i++) {
        lat[i] = .5 * (lat_bnds[i] + lat_bnds[i+1]);
        lon[i] = .5 * (lon_bnds[i] + lon_bnds[i+1]);
    }
    cmor_load_table("Tables/CMIP5_grids", &table_id);


    if (setup_grid(&grid_id, 
                   lon, lon_bnds,
                   lat, lat_bnds, 4) < 0) {

        assert(!"failed");
    }
    setup_mapping(grid_id);
    printf("test_rotated_pole(): DONE\n");
}
#endif /* TEST_MAIN2 */
