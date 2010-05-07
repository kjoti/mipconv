/*
 * calculator.c
 *
 * stack-oriented reverse-polish calculator.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "myutils.h"
#include "internal.h"


struct operator {
    int (*func)(void);
};
typedef struct operator operator_t;

struct operand {
    size_t size;
    double *values;
    double svalue;
    double miss;                /* ignored if size == 0 */
};
typedef struct operand operand_t;

#define MAX_OPRAND 128
static operand_t stack[MAX_OPRAND];
static int n_operands;


static int
set_operand(operand_t *x, size_t size, const float *values, double miss)
{
    double *p;
    int i;

    if (size > 0) {
        if ((p = malloc(sizeof(double) * size)) == NULL) {
            logging(LOG_SYSERR, NULL);
            return -1;
        }
        for (i = 0; i < size; i++)
            p[i] = values[i];

        x->size = size;
        x->values = p;
        x->miss = miss;
    } else {
        x->size = 0;
        x->values = NULL;
        x->svalue = miss;
    }
    return 0;
}


static int
set_operandd(operand_t *x, size_t size, const double *values, double miss)
{
    double *p;
    int i;

    if (size > 0) {
        if ((p = malloc(sizeof(double) * size)) == NULL) {
            logging(LOG_SYSERR, NULL);
            return -1;
        }
        for (i = 0; i < size; i++)
            p[i] = values[i];

        x->size = size;
        x->values = p;
        x->miss = miss;
    } else {
        x->size = 0;
        x->values = NULL;
        x->svalue = miss;
    }
    return 0;
}


static void
free_operand(operand_t *x)
{
    if (x->size > 0)
        free(x->values);
    x->size = 0;
    x->values = NULL;
}


static int
push_operand(const operand_t *x)
{
    if (n_operands >= MAX_OPRAND) {
        logging(LOG_ERR, "calc: Stack is full");
        exit(1);
        /* longjmp(); */
        /* return -1; */
    }
    stack[n_operands] = *x;
    n_operands++;
    return 0;
}


static int
pop_operand(operand_t *x)
{
    if (n_operands <= 0) {
        logging(LOG_ERR, "calc: Stack is empty");
        exit(1);
        /* longjmp(); */
        /* return -1; */
    }
    n_operands--;
    *x = stack[n_operands];
    return 0;
}


static int
exch(void)
{
    operand_t x1, x2;

    pop_operand(&x1);
    pop_operand(&x2);
    push_operand(&x1);
    push_operand(&x2);
    return 0;
}


static int
fdup(void)
{
    operand_t x, x2;

    pop_operand(&x);
    if (x.size == 0) {
        push_operand(&x);
    } else {
        set_operandd(&x2, x.size, x.values, x.miss);
        push_operand(&x2);
    }
    push_operand(&x);
    return 0;
}


/* f(x) = 1/x */
static int
reciprocal(void)
{
    operand_t x;
    int i;

    pop_operand(&x);

    if (x.size == 0) {
        if (x.svalue == 0) {
            logging(LOG_ERR, "Division by zero");
            exit(1);
            /* longjmp(); */
            /* return -1; */
        }
        x.svalue = 1. / x.svalue;
    } else {
        for (i = 0; i < x.size; i++)
            if (x.values[i] == 0. || x.values[i] == x.miss)
                x.values[i] = x.miss;
            else
                x.values[i] = 1. / x.values[i];
    }
    push_operand(&x);
    return 0;
}


/* f(x) = -x */
static int
negsign(void)
{
    operand_t x;
    int i;

    pop_operand(&x);

    if (x.size == 0)
        x.svalue *= -1;
    else
        for (i = 0; i < x.size; i++)
            if (x.values[i] != x.miss)
                x.values[i] *= -1.;

    push_operand(&x);
    return 0;
}


/* x**2 */
static int
square(void)
{
    operand_t x;
    int i;

    pop_operand(&x);

    if (x.size == 0)
        x.svalue *= x.svalue;
    else
        for (i = 0; i < x.size; i++)
            if (x.values[i] != x.miss)
                x.values[i] *= x.values[i];

    push_operand(&x);
    return 0;
}


static int
fsqrt(void)
{
    operand_t x;
    int i;

    pop_operand(&x);
    if (x.size == 0) {
        if (x.svalue < 0.) {
            logging(LOG_ERR, "sqrt(x) for x < 0");
            return -1;
        }
        x.svalue = sqrt(x.svalue);
    } else
        for (i = 0; i < x.size; i++)
            x.values[i] = x.values[i] >= 0. ? sqrt(x.values[i]) : x.miss;

    push_operand(&x);
    return 0;
}


static int
flog10(void)
{
    operand_t x;
    int i;

    pop_operand(&x);
    if (x.size == 0) {
        if (x.svalue <= 0.) {
            logging(LOG_ERR, "log10(x) for x < 0");
            return -1;
        }
        x.svalue = log10(x.svalue);
    } else
        for (i = 0; i < x.size; i++)
            x.values[i] = x.values[i] > 0. ? log10(x.values[i]) : x.miss;

    push_operand(&x);
    return 0;
}


static int
flog(void)
{
    operand_t x;
    int i;

    pop_operand(&x);
    if (x.size == 0) {
        if (x.svalue <= 0.) {
            logging(LOG_ERR, "log(x) for x < 0");
            return -1;
        }
        x.svalue = log(x.svalue);
    } else
        for (i = 0; i < x.size; i++)
            x.values[i] = x.values[i] > 0. ? log(x.values[i]) : x.miss;

    push_operand(&x);
    return 0;
}




/* x1 OP x2 */
#define BINOP(NAME__, OP__) \
static int \
NAME__(void) \
{ \
    operand_t x1, x2; \
    int i; \
    pop_operand(&x2); \
    pop_operand(&x1); \
    if (x1.size > 0 && x2.size > 0 && x1.size != x2.size) { \
        logging(LOG_ERR, "operand size mismatch"); \
        return -1; \
    } \
    if (x1.size == 0 && x2.size > 0) { \
        operand_t temp; \
        temp = x1; \
        x1 = x2; \
        x2 = temp; \
    } \
    if (x1.size == 0) { \
        /* scalar OP scalar */ \
        x1.svalue OP__ x2.svalue; \
    } else if (x2.size == 0) { /* vector OP scalar */ \
        for (i = 0; i < x1.size; i++) \
            if (x1.values[i] != x1.miss) \
                x1.values[i] OP__ x2.svalue; \
    } else { /* vector + vector */ \
        for (i = 0; i < x1.size; i++) { \
            if (x1.values[i] != x1.miss && x2.values[i] != x2.miss) \
                x1.values[i] OP__ x2.values[i]; \
            if (x2.values[i] == x2.miss) \
                x1.values[i] = x1.miss; \
        } \
    } \
    push_operand(&x1); \
    free_operand(&x2); \
    return 0; \
}

BINOP(add, +=)
BINOP(mul, *=)


static int
sub(void)
{
    negsign();
    return add();
}

static int
fdiv(void)
{
    reciprocal();
    return mul();
}


#define COMPFUNC(NAME__, OP__) \
static int \
NAME__(void) \
{ \
    operand_t x1, x2; \
    int i; \
    pop_operand(&x2); \
    pop_operand(&x1); \
    if (x1.size > 0 && x2.size > 0 && x1.size != x2.size) \
        return -1; \
    if (x1.size == 0 && x2.size == 0) { \
        assert(x2.size == 0); \
        if (x2.svalue OP__ x1.svalue) \
            x1.svalue = x2.svalue; \
    } else if (x1.size == 0 && x2.size > 0) { \
        operand_t temp; \
        for (i = 0; i < x2.size; i++) \
            if (x2.values[i] != x2.miss && x1.svalue OP__ x2.values[i]) \
                x2.values[i] = x1.svalue; \
        temp = x2; \
        x2 = x1; \
        x1 = temp; \
    } else if (x1.size > 0 && x2.size == 0) { \
        for (i = 0; i < x1.size; i++) \
            if (x1.values[i] != x1.miss && x2.svalue OP__ x1.values[i]) \
                x1.values[i] = x2.svalue; \
    } else { \
        for (i = 0; i < x1.size; i++) { \
            if ( (   x1.values[i] != x1.miss \
                  && x2.values[i] != x2.miss \
                  && x2.values[i] OP__ x1.values[i] ) \
                 || (   x1.values[i] == x1.miss \
                     && x2.values[i] != x2.miss ) ) \
                x1.values[i] = x2.values[i]; \
        } \
    } \
    push_operand(&x1); \
    free_operand(&x2); \
    return 0; \
}

COMPFUNC(maxf, >)
COMPFUNC(minf, <)


static int
mask(void)
{
    operand_t x, mask;
    int i;

    pop_operand(&mask);
    pop_operand(&x);
    if (mask.size == 0 || x.size == 0 || mask.size != x.size)
        return -1;

    for (i = 0; i < x.size; i++)
        if (mask.values[i] == mask.miss)
            x.values[i] = x.miss;

    push_operand(&x);
    free_operand(&mask);
    return 0;
}


static int
power(void)
{
    operand_t expo, x;
    int i, eflag;
    unsigned flag;

    pop_operand(&expo);
    pop_operand(&x);

    eflag = 0;
    flag = (x.size > 0 ? 1U : 0U) | (expo.size > 0 ? 2U : 0U);
    switch (flag) {
    case 0:
        x.svalue = pow(x.svalue, expo.svalue);
        break;
    case 1:
        for (i = 0; i < x.size; i++)
            if (x.values[i] != x.miss && x.values[i] >= 0.)
                x.values[i] = pow(x.values[i], expo.svalue);
            else
                x.values[i] = x.miss;
        break;
    case 2:
        eflag = 1;
        break;
    case 3:
        if (x.size == expo.size) {
            for (i = 0; i < x.size; i++)
                if (x.values[i] != x.miss && x.values[i] > 0.)
                    x.values[i] = pow(x.values[i], expo.values[i]);
                else
                    x.values[i] = x.miss;
        } else {
            eflag = 1;
        }
        break;
    }

    if (eflag)
        return -1;

    push_operand(&x);
    free_operand(&expo);
    return 0;
}


static int
get_operand(operand_t *x, const char *str)
{
    double val;
    char *endptr;

    val = strtod(str, &endptr);
    if (*endptr != '\0')
        return -1;

    x->size = 0;
    x->values = NULL;
    x->svalue = val;
    return 0;
}


static operator_t
get_operator(const char *str)
{
    struct {
        const char *key;
        int (*func)(void);
    } tab[] = {
        { "+",   add  },
        { "*",   mul  },
        { "mul", mul  },
        { "-",   sub  },
        { "/",   fdiv },
        { "min",   minf },
        { "max",   maxf },
        { "square", square },
        { "sqrt",   fsqrt  },
        { "log10",   flog10  },
        { "log",   flog  },
        { "exch", exch },
        { "dup", fdup },
        { "mask", mask },
        { "pow", power }
    };
    operator_t op;
    int i;

    op.func = NULL;

    for (i = 0; i < sizeof tab / sizeof tab[0]; i++)
        if (strcmp(tab[i].key, str) == 0) {
            op.func = tab[i].func;
            break;
        }
    return op;
}


int
eval_calc(const char *expr, float *data, double miss, size_t size)
{
    operand_t x, temp;
    operator_t op;
    int i, num, rval = -1;
    const char *tail;
    char *endptr;
    char buf[64];

    if (expr == NULL)
        return 0;

    /* if (setjmp(eval_buf)) return -1; */

    /*
     * push the first operands.
     */
    if (set_operand(&x, size, data, miss) < 0)
        return -1;
    push_operand(&x);

    /*
     * process expressions.
     */
    tail = expr + strlen(expr);

    while (expr < tail) {
        num = split(buf, sizeof buf, 1, expr, tail, &endptr);
        if (num < 0) {
            logging(LOG_ERR, "invalid expr: %s", expr);
            goto finish;
        }
        if (num == 1) {
            op = get_operator(buf);
            if (op.func) {
                if (op.func() < 0)
                    goto finish;
            } else {
                if (get_operand(&temp, buf) < 0) {
                    logging(LOG_ERR, "invalid operand: %s", buf);
                    goto finish;
                }
                push_operand(&temp);
            }
        }
        expr = (const char *)endptr;
    }

    /*
     * pop the result.
     */
    pop_operand(&x);
    assert(x.size == size);
    for (i = 0; i < size; i++)
        data[i] = (float)x.values[i];
    rval = 0;

finish:
    free_operand(&x);
    return rval;
}


#ifdef TEST_MAIN2
void
test1(void)
{
    operand_t x;

    set_operand(&x, 0, NULL, 1.);
    push_operand(&x);
    set_operand(&x, 0, NULL, 3.);
    push_operand(&x);

    add();
    pop_operand(&x);
    assert(x.size == 0 && x.svalue == 4.);

    push_operand(&x);
    negsign();
    pop_operand(&x);
    assert(x.size == 0 && x.svalue == -4.);

    push_operand(&x);
    reciprocal();
    pop_operand(&x);
    assert(x.size == 0 && x.svalue == -0.25);
}



void
test2(void)
{
    operand_t x;
    float v[] = {2.f, 3.f, 5.f, -999.f, 13.f};

    set_operand(&x, 5, v, -999.);
    push_operand(&x);
    set_operand(&x, 0, NULL, 1.);
    push_operand(&x);
    add();
    pop_operand(&x);
    assert(x.size == 5);
    assert(x.values[0] == 3.f);
    assert(x.values[1] == 4.f);
    assert(x.values[2] == 6.f);
    assert(x.values[3] == -999.f);
    assert(x.values[4] == 14.f);

    push_operand(&x);
    set_operand(&x, 5, v, -999.);
    push_operand(&x);
    add();
    pop_operand(&x);
    assert(x.values[0] == 2.f + 3.f);
    assert(x.values[1] == 3.f + 4.f);
    assert(x.values[2] == 5.f + 6.f);
    assert(x.values[3] == -999.f);
    assert(x.values[4] == 13.f + 14.f);
}


void
test3(void)
{
    operand_t x;
    float v1[] = {1.f, -999.f, 3.f, -999.f, 5.f };
    float v2[] = {10.f, 20.f, -999.f, -999.f, 50.f };

    set_operand(&x, 5, v1, -999.);
    push_operand(&x);
    set_operand(&x, 5, v2, -999.);
    push_operand(&x);
    add();
    pop_operand(&x);

    assert(x.values[0] == 11.f);
    assert(x.values[1] == -999.f);
    assert(x.values[2] == -999.f);
    assert(x.values[3] == -999.f);
    assert(x.values[4] == 55.f);

    push_operand(&x);
    set_operand(&x, 5, v2, -999.);
    push_operand(&x);
    sub();
    pop_operand(&x);
    assert(x.values[0] == 1.f);
    assert(x.values[1] == -999.f);
    assert(x.values[2] == -999.f);
    assert(x.values[3] == -999.f);
    assert(x.values[4] == 5.f);
}


void
test4(void)
{
    operand_t x;
    float v1[] = { 0.f, 1.f, 2.f, 4.f, 8.f };

    set_operand(&x, 5, v1, -999.);
    push_operand(&x);
    reciprocal();
    pop_operand(&x);

    assert(x.values[0] == -999.f);
    assert(x.values[1] == 1.f);
    assert(x.values[2] == 0.5f);
    assert(x.values[3] == 0.25f);
    assert(x.values[4] == 0.125f);

    push_operand(&x);
    negsign();
    pop_operand(&x);

    assert(x.values[0] == -999.f);
    assert(x.values[1] == -1.f);
    assert(x.values[2] == -0.5f);
    assert(x.values[3] == -0.25f);
    assert(x.values[4] == -0.125f);
}


void
test5(void)
{
    operand_t x;
    float v1[] = {1.f, 2.f, -999.f, 8.f, 16.f };
    float v2[] = {0.f, 1.f, 2.f, -999.f, 4.f };

    set_operand(&x, 5, v1, -999.);
    push_operand(&x);
    set_operand(&x, 5, v2, -999.);
    push_operand(&x);

    fdiv();
    pop_operand(&x);
    assert(x.values[0] == -999.f);
    assert(x.values[1] == 2.f);
    assert(x.values[2] == -999.f);
    assert(x.values[3] == -999.f);
    assert(x.values[4] == 4.f);

    push_operand(&x);
    set_operand(&x, 0, NULL, -0.5);
    push_operand(&x);
    fdiv();
    pop_operand(&x);
    assert(x.values[0] == -999.f);
    assert(x.values[1] == -4.f);
    assert(x.values[2] == -999.f);
    assert(x.values[3] == -999.f);
    assert(x.values[4] == -8.f);
}


void
test6(void)
{
    float v[] = {1.f, 2.f, -999.f, 4.f, 5.f };
    int rval;

    rval = eval_calc("1. + 1e4 *", v, -999., 5);
    assert(rval == 0);

    assert(v[0] == 2e4f);
    assert(v[1] == 3e4f);
    assert(v[2] == -999.f);
    assert(v[3] == 5e4f);
    assert(v[4] == 6e4f);
}


void
test7(void)
{
    operand_t x;
    float v1[] = {1.f,  2.f, -999.f, 4.f,    -999.f };
    float v2[] = {1.1f, 1.9f, 3.f,   -999.f, -999.f };

    /*
     * test of maxf.
     */
    set_operand(&x, 5, v1, -999.);
    push_operand(&x);
    set_operand(&x, 5, v2, -999.);
    push_operand(&x);

    maxf();
    pop_operand(&x);
    assert(x.values[0] == 1.1f);
    assert(x.values[1] == 2.f);
    assert(x.values[2] == 3.f);
    assert(x.values[3] == 4.f);
    assert(x.values[4] == -999.f);
    free_operand(&x);

    /*
     * test of minf.
     */
    set_operand(&x, 5, v1, -999.);
    push_operand(&x);
    set_operand(&x, 5, v2, -999.);
    push_operand(&x);

    minf();
    pop_operand(&x);
    assert(x.values[0] == 1.f);
    assert(x.values[1] == 1.9f);
    assert(x.values[2] == 3.f);
    assert(x.values[3] == 4.f);
    assert(x.values[4] == -999.f);
    free_operand(&x);
}


void
test8(void)
{
    float v[] = { -1.f, 0.f, .25f, -999.f, 1.f, 2.f };
    int rval;

    rval = eval_calc("0. max 1. min", v, -999., 6);
    assert(rval == 0);
    assert(v[0] == 0.f);
    assert(v[1] == 0.f);
    assert(v[2] == .25f);
    assert(v[3] == -999.f);
    assert(v[4] == 1.f);
    assert(v[5] == 1.f);
}


void
test9(void)
{
    float v[] = { -0.1f, 0.f, 4.f, 9.f, -999.f };
    int rval;
    float miss = -999.f;

    rval = eval_calc("0.5 pow", v, miss, 5);
    assert(rval == 0);
    assert(v[0] == miss);
    assert(v[1] == 0.f);
    assert(fabsf(v[2] - 2.f) < 1e-6);
    assert(fabsf(v[3] - 3.f) < 1e-6);
    assert(v[4] == miss);
}


int
test_calculator(int argc, char **argv)
{
    open_logging(stderr, "calculator");
    test1();
    test2();
    test3();
    test4();
    test5();
    test6();
    test7();
    test8();
    test9();
    printf("test_calculator(): DONE\n");
    return 0;
}
#endif
