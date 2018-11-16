/*
 * iarray.c
 */
#include <sys/types.h>
#include "myutils.h"


int
iarray_cmp(const int *a, const int *b, size_t nelems)
{
    for (; nelems > 0; a++, b++, nelems--)
        if (*a != *b)
            return *a - *b;

    return 0;
}


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


/*
 * remove 'num' elements at 'pos'.
 */
void
iarray_remove(int *array, size_t size, int pos, size_t num)
{
    int *src, *dest, *tail;

    dest = array + pos;
    src = array + pos + num;
    tail = array + size;
    while (src < tail)
        *dest++ = *src++;
}
