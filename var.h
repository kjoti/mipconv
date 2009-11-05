#ifndef VAR_H
#define VAR_H

#include "gtool3.h"


#define MAX_NDIM 7

/*
 *  A scalar variable (rank == 0) is not supported.
 *  'rank == 0' means that this variable is not initialized and allocated.
 *
 */
struct variable {
    int rank;                   /* usually time-axis not included. */
    int dimlen[MAX_NDIM];
    float *data;
    char typecode;              /* typecode must be 'f' */
    int timedepend;             /* 0 == independent of time */
    double timebnd[2];
    double time;

    char *title;          /* original title */
    char *unit;           /* original unit */
};

typedef struct variable myvar_t;


myvar_t *new_var(void);
void free_var(myvar_t *var);
int resize_var(myvar_t *var, const int *dimlen, int ndim);
int read_var(myvar_t *var, GT3_Varbuf *vbuf);

#endif /* !VAR_H */
