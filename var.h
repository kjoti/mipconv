#ifndef VAR_H
#define VAR_H

#include "gtool3.h"
#include "seq.h"


/*
 * A scalar variable (rank == 0) is not supported.
 */
struct variable {
    int dimlen[3];
    float *data;                /* XXX use FLOAT */
    size_t nelems;              /* the number of elements of data */
    char typecode;              /* typecode must be 'f' */

    /*
     * 0: independent of time
     * 1: snapshot
     * 2: time-mean
     */
    int timedepend;
    double timebnd[2];
    double time;

    char *title;          /* original title */
    char *unit;           /* original unit */
};

typedef struct variable myvar_t;


myvar_t *new_var(void);
void free_var(myvar_t *var);
int resize_var(myvar_t *var, const int *dimlen, int ndim);
int read_var(myvar_t *var, GT3_Varbuf *vbuf, struct sequence *zseq);

#endif /* !VAR_H */
