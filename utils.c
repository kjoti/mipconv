#include <assert.h>
#include "internal.h"


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


int
get_axis_prof(char *name, int *istr, int *iend,
              const GT3_HEADER *head, int idx)
{
    static const char *aitm[] = { "AITM1", "AITM2", "AITM3" };
    static const char *astr[] = { "ASTR1", "ASTR2", "ASTR3" };
    static const char *aend[] = { "AEND1", "AEND2", "AEND3" };

    assert(idx >= 0 && idx < 3);

    if (GT3_copyHeaderItem(name, 17, head, aitm[idx]) == NULL
        || GT3_decodeHeaderInt(istr, head, astr[idx]) < 0
        || GT3_decodeHeaderInt(iend, head, aend[idx]) < 0)
        return -1;

    return 0;
}
