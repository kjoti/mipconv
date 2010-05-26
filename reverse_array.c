/*
 * reverse_array.c
 */
#include <sys/types.h>
#include "myutils.h"


void
reverse_iarray(int *head, size_t len)
{
    int *tail = head + len - 1;
    int temp;

    while (head < tail) {
        temp = *tail;
        *tail = *head;
        *head = temp;
        head++;
        tail--;
    }
}
