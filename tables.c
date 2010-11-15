/*
 * tables.c
 *
 * mipconv uses at most two tables (one for usual variables, another
 * for grid mapping). As long as simple lat/lon coordinates,
 * the table for grid mapping is not needed.
 */
#include "cmor.h"
#include "logging.h"

/*
 * IDs of two tables.
 */
static int grid_table = -1;
static int normal_table = -1;


static int
switch_table(int table)
{
    if (table < 0) {
        logging(LOG_ERR, "requested table not setup yet.");
        return -1;
    }
    cmor_set_table(table);
    return 0;
}


int
switch_to_grid_table(void)
{
    return switch_table(grid_table);
}


int
switch_to_normal_table(void)
{
    return switch_table(normal_table);
}


static int
load_table(const char *path, int *table_id)
{
    int id;

    if (cmor_load_table((char *)path, &id) != 0) {
        logging(LOG_ERR, "cmor_load_table() failed.");
        return -1;
    }

    logging(LOG_INFO, "loaded (%s): table_id = %d", path, id);
    *table_id = id;
    return 0;
}


int
load_grid_table(const char *path)
{
    return load_table(path, &grid_table);
}


int
load_normal_table(const char *path)
{
    return load_table(path, &normal_table);
}
