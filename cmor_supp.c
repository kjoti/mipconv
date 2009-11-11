/*
 *  cmor_supp.c
 *
 *  Supplement code for CMOR2.
 *  XXX: This depends on the internal of CMOR2.
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"

#include "cmor.h"
#include "cmor_supp.h"

#include "internal.h"


static cmor_table_t *
get_default_table(void)
{
    return (CMOR_TABLE < 0) ? NULL : &cmor_tables[CMOR_TABLE];
}

/*
 *  lookup vardef from MIP table
 */
cmor_var_def_t *
lookup_vardef(const char *name)
{
    cmor_table_t *table;
    cmor_var_def_t *ptr;
    int n;

    if ((table = get_default_table()) == NULL) {
        logging(LOG_ERR, "No table established");
        return NULL;
    }

    for (n = 0, ptr = table->vars; n < table->nvars; n++, ptr++)
        if (strcmp(ptr->id, name) == 0)
            return ptr;


    logging(LOG_WARN, "No such variable: %s", name);
    return NULL; /* not found */
}


/*
 *  lookup axisdef by standard-name(not id) from MIP table
 */
cmor_axis_def_t *
lookup_axisdef(const char *name)
{
    cmor_table_t *table;
    cmor_axis_def_t *ptr;
    int n;

    if ((table = get_default_table()) == NULL) {
        logging(LOG_ERR, "No table established");
        return NULL;
    }

    for (n = 0, ptr = table->axes; n < table->naxes; n++, ptr++)
        if (strcmp(ptr->standard_name, name) == 0)
            return ptr;

    logging(LOG_WARN, "No such axis: %s", name);
    return NULL;
}

/*
 *  get axis_def for the specified variable definition.
 */
cmor_axis_def_t *
get_axisdef_in_vardef(const cmor_var_def_t *vdef, int n)
{
    cmor_table_t *table;
    cmor_axis_def_t *axisdef;

    if (vdef == NULL || n < 0 || n >= vdef->ndims)
        return NULL;

    table = &cmor_tables[vdef->table_id];
    assert(table);

    axisdef = &table->axes[vdef->dimensions[n]];
    assert(axisdef);

    return axisdef;
}

/*
 *  lookup axis_def in var_def.
 */
cmor_axis_def_t *
lookup_axisdef_in_vardef(const char *name, const cmor_var_def_t *vdef)
{
    cmor_axis_def_t *adef;
    int i;

    if (vdef)
        for (i = 0; i < vdef->ndims; i++) {
            adef = get_axisdef_in_vardef(vdef, i);

            if (strcmp(adef->standard_name, name) == 0)
                return adef;
        }
    return NULL;
}


int
is_singleton(const cmor_axis_def_t *adef)
{
    return adef->value != 1.e20;
}


#ifdef TEST_MAIN2
int
test_cmor_supp(void)
{
    cmor_var_def_t *vdef;
    cmor_axis_def_t *adef;

    vdef = lookup_vardef("tas");
    assert(vdef);
    assert(vdef->ndims == 4);
    adef = lookup_axisdef_in_vardef("height", vdef);
    assert(adef);
    assert(is_singleton(adef));
    assert(vdef->positive == '\0');

    vdef = lookup_vardef("tauu");
    assert(vdef);
    assert(vdef->ndims == 3);
    assert(vdef->positive == 'd');

    vdef = lookup_vardef("hfls");
    assert(vdef);
    assert(vdef->ndims == 3);
    assert(vdef->positive == 'u');

    vdef = lookup_vardef("cl");
    assert(vdef);
    assert(vdef->ndims == 4);
    assert(vdef->positive == '\0');

    adef = get_axisdef_in_vardef(vdef, 0);
    assert(adef->axis == 'T');
    adef = get_axisdef_in_vardef(vdef, 1);
    assert(adef->axis == 'Z');
    adef = get_axisdef_in_vardef(vdef, 2);
    assert(adef->axis == 'Y');
    adef = get_axisdef_in_vardef(vdef, 3);
    assert(adef->axis == 'X');

    adef = lookup_axisdef_in_vardef("latitude", vdef);
    assert(adef);
    assert(adef->must_have_bounds);

    printf("test_cmor_supp(): DONE\n");
    return 0;
}
#endif /* TEST_MAIN2 */
