/*
 * cmor_supp.h
 */
#ifndef CMOR_SUPP_H
#define CMOR_SUPP_H

#ifdef __cplusplus
extern "C" {
#endif

cmor_var_def_t *lookup_vardef(const char *name, cmor_table_t *table);
cmor_axis_def_t *get_axisdef_in_vardef(const cmor_var_def_t *vdef, int n);
int is_singleton(const cmor_axis_def_t *adef);

#ifdef __cplusplus
}
#endif
#endif
