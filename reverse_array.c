/*
 * iarray.c
 */
#include <sys/types.h>
#include "myutils.h"


void
iarray_reverse(int *head, size_t nelems)
{
    int *tail = head + nelems - 1;
    int temp;

    while (head < tail) {
        temp = *tail;
        *tail = *head;
        *head = temp;
        head++;
        tail--;
    }
}


int
iarray_find_first(const int *values, size_t nelems, int key)
{
    int i;

    for (i = 0; i < nelems; i++)
        if (values[i] == key)
            return i;
    return -1;
}
