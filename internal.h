/*
 *  internal.h
 */
#ifndef INTERNAL_H
#define INTERNAL_H

#include "cmor.h"
#include "seq.h"

/* setup.c */
void setup(void);

/* axis.c */
int get_axis_ids(int *ids, int *nids,
                 const char *aitm, int astr, int aend,
                 struct sequence *zslice,
                 const cmor_var_def_t *vdef);
int dummy_dimname(const char *name);


int get_axis_by_gt3name(const char *name, int astr, int aend);

/* converter.c */
int convert(const char *varname, const char *inputfile, int cnt);

#endif /* !INTERNAL_H */
