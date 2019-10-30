/*
 * site.h
 */
#ifndef SITE_H
#define SITE_H

#include <sys/types.h>

struct site_locations_t {
    size_t nlocs;               /* the number of locations */
    int *ids;                   /* each ID */
    double *lons;               /* each longitude */
    double *lats;               /* each latitude */

    int *indexes;
};
typedef struct site_locations_t site_locations;

void free_site_locations(site_locations *locs);
site_locations *load_site_locations(const char *path);
int update_site_indexes(site_locations *sites, const GT3_HEADER *head);

#endif /* !SITE_H */
