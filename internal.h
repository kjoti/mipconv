/*
 *  internal.h
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

/* setup.c */
int set_outdir(const char *dir);
int read_config(FILE *fp);
void setup(void);

/* axis.c */
int get_axis_ids(int *ids, int *nids,
                 const char *aitm, int astr, int aend,
                 struct sequence *zslice,
                 const cmor_var_def_t *vdef);
int dummy_dimname(const char *name);


int get_axis_by_gt3name(const char *name, int astr, int aend);

/* timeaxis.c */
int set_calendar_by_name(const char *name);
double get_time(const GT3_Date *date);
void step_time(GT3_Date *date);
int get_timeaxis(const cmor_axis_def_t *timedef);
void set_origin_year(int year);
int set_timeinterval(const char *freq);


/* converter.c */
int convert(const char *varname, const char *inputfile, int cnt);

int set_axis_slice(int idx, const char *spec);

/* logicline.c */
size_t read_logicline(char *dest, size_t ndest, FILE *fp);

/* zfactor.c */
int setup_zfactors(int *zfac_ids, int var_id, const GT3_HEADER *head);

/* utils.c */
void reverse_iarray(int *ia, int num);
int get_axis_prof(char *name, int *istr, int *iend,
                  const GT3_HEADER *head, int idx);


#endif /* !INTERNAL_H */
