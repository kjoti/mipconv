/*
 * sdb.c -- simple database (plain text).
 */
#include <string.h>
#include <stdio.h>

#include "internal.h"
#include "logging.h"
#include "myutils.h"


static FILE *db = NULL;


int
sdb_close(void)
{
    int rval = 0;

    if (db && (rval = fclose(db)) < 0)
        logging(LOG_SYSERR, NULL);

    db = NULL;
    return rval;
}


int
sdb_open(const char *path)
{
    sdb_close();

    if ((db = fopen(path, "r")) == NULL) {
        logging(LOG_SYSERR, path);
        return -1;
    }
    logging(LOG_INFO, "open a database file (%s).", path);
    return 0;
}


char *
sdb_readitem(const char *item)
{
    char aline[4096], key[32];
    char *ptr;
    char *value = NULL;

    if (!db)
        return NULL;

    rewind(db);
    while (!feof(db)) {
        read_logicline(aline, sizeof aline, db);

        /* skip a comment line or a blank line */
        if (aline[0] == '#' || aline[0] == '\0')
            continue;

        ptr = split2(key, sizeof key, aline, " \t");
        if (strcmp(key, item) == 0) {
            value = strdup(ptr);
            if (value == NULL)
                logging(LOG_SYSERR, NULL);
            break;
        }
    }
    return value;
}


#if 0
int
test_sdb(void)
{
    sdb_open("conf/20c3m.conf");

    printf("(%s)\n", sdb_readitem("comment"));

    sdb_free();
    return 0;
}
#endif
