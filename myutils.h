#ifndef MYUTILS__H
#define MYUTILS__H

int split(char *buf, int maxlen, int maxnum,
          const char *head, const char *tail, char **endptr);
int get_ints(int vals[], int maxnum, const char *str, char delim);
int copysubst(char *dest, size_t len,
              const char *orig, const char *old, const char *new);
int startswith(const char *s1, const char *s2);

#endif /* !MYUTILS__H */
