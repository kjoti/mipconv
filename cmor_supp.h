/*
 * cmor_supp.h
 */
#ifndef CMOR_SUPP_H
#define CMOR_SUPP_H

#ifdef __cplusplus
extern "C" {
#endif

cmor_var_def_t *lookup_vardef(const char *name);
cmor_var_def_t *lookup_formula_vardef(const char *name);
cmor_axis_def_t *lookup_axisdef(const char *name);

cmor_axis_def_t *get_axisdef_in_vardef(const cmor_var_def_t *vdef, int n);
cmor_axis_def_t *lookup_axisdef_in_vardef(const char *name,
                                          const cmor_var_def_t *vdef);
int is_singleton(const cmor_axis_def_t *adef);
const char *get_frequency(const cmor_var_def_t *vdef);
int has_modellevel_dim(const cmor_var_def_t *vdef);
int lookup_varid(const char *name);

void set_missing_value(int var_id, double value);
void set_var_attrib_via_cur_dataset(int var_id, const char *key);

#ifdef __cplusplus
}
#endif
#endif
