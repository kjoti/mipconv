/*
 * internal.h
 */
#ifndef INTERNAL_H
#define INTERNAL_H

#include <stdio.h>

#include "gtool3.h"
#include "cmor.h"
#include "seq.h"

#ifndef CMOR_MAX_DIMENSIONS
#  define CMOR_MAX_DIMENSIONS 7
#endif

struct gtool3_dim_prop {
    char aitm[17];
    int astr, aend;
};
typedef struct gtool3_dim_prop gtool3_dim_prop;

/* setup.c */
int use_netcdf(int v);
int set_writing_mode(const char *str);
int set_outdir(const char *dir);
int read_config(FILE *fp);
int setup(void);

/* tables.c */
int switch_to_grid_table(void);
int switch_to_normal_table(void);
int load_grid_table(const char *path);
int load_normal_table(const char *path);

/* axis.c */
int get_axis_ids(int *ids, int *nids,
                 const char *aitm, int astr, int aend,
                 struct sequence *zslice,
                 const cmor_var_def_t *vdef);
int dummy_dimname(const char *name);


int get_axis_by_gt3name(const char *name, int astr, int aend);

/* timeaxis.c */
void set_origin_year(int year);
int set_calendar_by_name(const char *name);
int set_calendar(int cal);
double get_time(const GT3_Date *date);
int get_calendar(void);
void step_time(GT3_Date *date, const GT3_Duration *tdur);
int get_timeaxis(const cmor_axis_def_t *timedef);
int check_duration(const GT3_Duration *tdur,
                   const GT3_Date *date1,
                   const GT3_Date *date2);

/* converter.c */
int get_dim_prop(gtool3_dim_prop *dim, const GT3_HEADER *head, int idx);
int set_axis_slice(int idx, const char *spec);
void unset_axis_slice(void);
int set_calcexpr(const char *str);
void unset_calcexpr(void);
int set_positive(const char *str);
void unset_positive(void);
int set_grid_mapping(const char *name);
int convert(const char *varname, const char *inputfile, int cnt);

/* zfactor.c */
int setup_zfactors(int *zfac_ids, int var_id,
                   const GT3_HEADER *head,
                   const struct sequence *zslice);

/* unit.c */
int set_varunit(const char *str);
void unset_varunit(void);
int rewrite_unit(char *unit, size_t size);

/* calculator.c */
int eval_calc(const char *expr, float *data, double miss, size_t size);

/* sdb.c */
int sdb_close(void);
int sdb_open(const char *path);
char *sdb_readitem(const char *item);

/* coord.c */
int rotate_lonlat(double *lon, double *lat,
                  const double *rlon, const double *rlat,
                  int nlon, int nlat,
                  double phi, double theta, double psi);

/* rotated_pole.c */
int setup_rotated_pole(int *grid_id, const gtool3_dim_prop *dims);

/* version.c */
char *mipconv_version(void);

#endif /* !INTERNAL_H */
