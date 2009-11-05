/*
 *  internal.h
 */
#ifndef INTERNAL_H
#define INTERNAL_H

#include "gtool3.h"

/* setup.c */
void setup(void);

/* axis.c */
int get_axis_by_gt3name(const char *name, int astr, int aend);
int get_axis_by_gtool3(const GT3_HEADER *head, int idx);

/* converter.c */
int convert(const char *varname, const char *inputfile, int cnt);

#endif /* !INTERNAL_H */
