#include "utils.h"

void
reverse_iarray(int *ia, int num)
{
    int temp, head, tail;

    head = 0;
    tail = num - 1;
    while (tail > head) {
        temp = ia[head];
        ia[head] = ia[tail];
        ia[tail] = temp;
        head++;
        tail--;
    }
}
