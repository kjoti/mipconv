#ifndef MYUTILS__H
#define MYUTILS__H
#include <sys/types.h>
#include <stdio.h>

int split(char *buf, int maxlen, int maxnum,
          const char *head, const char *tail, char **endptr);
char *split2(char *key, size_t keylen, const char *ptr, const char *delim);
int get_ints(int vals[], int maxnum, const char *str, char delim);
int copysubst(char *dest, size_t len,
              const char *orig, const char *old, const char *new);
int startswith(const char *s1, const char *s2);

size_t read_logicline(char *dest, size_t ndest, FILE *fp);

void reverse_iarray(int *ia, size_t num);


#endif /* !MYUTILS__H */
