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
 *  from MIP table
 */
cmor_var_def_t *
lookup_vardef(const char *name, cmor_table_t *table)
{
    cmor_var_def_t *var;
    int n;

    if (table == NULL && (table = get_default_table()) == NULL) {
        logging(LOG_ERR, "No table established");
        return NULL;
    }

    for (n = 0; n < table->nvars; n++) {
        var = &table->vars[n];

        if (strcmp(var->id, name) == 0)
            return var;
    }

    logging(LOG_WARN, "No such variable: %s", name);
    return NULL; /* not found */
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


int
is_singleton(const cmor_axis_def_t *adef)
{
    return adef->value != 1.e20;
}


/*
 *
 */
int
find_axisdef_in_var(const cmor_var_def_t *var, const char *id)
{
    cmor_table_t *table;
    int i, axis_id;

    if (var == NULL)
        return 0;

    table = &cmor_tables[var->table_id];
    assert(table);

    for (i = 0; i < var->ndims; i++) {
        axis_id = var->dimensions[i];

        if (axis_id >= 0 && strcmp(table->axes[axis_id].id, id) == 0)
            return 1;
    }
    return 0;
}


#ifdef TEST_MAIN2
int
test_cmor_supp(void)
{
    cmor_var_def_t *vdef;
    cmor_axis_def_t *adef;
    int i;


    vdef = lookup_vardef("tas", NULL);
    assert(vdef);
    printf("%d\n", vdef->ndims);
    for (i = 0; i < vdef->ndims; i++) {
        adef = get_axisdef_in_vardef(vdef, i);
        assert(adef);
        printf("%d (%c) %s %g\n", i, adef->axis,
               adef->standard_name,
               adef->value);
    }


    return 0;
}
#endif /* TEST_MAIN2 */
