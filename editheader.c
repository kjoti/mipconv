/*
 * editheader.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "gtool3.h"
#include "myutils.h"

#define MAX_ENTRY 8
static struct {
    char *key;
    char *value;
} edittab[MAX_ENTRY];
static int nentry = 0;

static const char *editable[] = {
    "ITEM",
    "TITLE",
    "UNIT",
    "AITM1",
    "AITM2",
    "AITM3",
};


void
unset_header_edit(void)
{
    int i;

    for (i = 0; i < nentry; i++) {
        free(edittab[i].key);
        free(edittab[i].value);
    }
    nentry = 0;
}


int
set_header_edit(const char *str)
{
    char key[16], *value;
    int i;

    if (nentry >= MAX_ENTRY) {
        logging(LOG_ERR, "Too many entry to edit header.");
        return -1;
    }

    value = split2(key, sizeof key, str, ":");
    toupper_string(key);
    for (i = 0; i < sizeof editable / sizeof editable[0]; i++)
        if (strcmp(key, editable[i]) == 0)
            break;

    if (i == sizeof editable / sizeof editable[0]) {
        logging(LOG_ERR, "%s: cannot edit this item.", key);
        return -1;
    }
    edittab[nentry].key = strdup(key);
    edittab[nentry].value = strdup(value);
    if (edittab[nentry].key == NULL || edittab[nentry].value == NULL) {
        logging(LOG_SYSERR, NULL);
        return -1;
    }
    nentry++;
    return 0;
}


int
edit_header(GT3_HEADER *head)
{
    int i;

    for (i = 0; i < nentry; i++) {
        GT3_setHeaderString(head, edittab[i].key, edittab[i].value);
        logging(LOG_INFO, "change header(%s) to %s.",
                edittab[i].key, edittab[i].value);
    }
    return 0;
}


#ifdef TEST_MAIN
#include <assert.h>

void
test1(void)
{
    GT3_HEADER head;
    char buf[33];

    assert(nentry == 0);
    set_header_edit("item:CLDFRC");
    assert(nentry == 1);
    set_header_edit("aitm3:ISCCPTP49");
    assert(nentry == 2);

    GT3_initHeader(&head);
    edit_header(&head);

    GT3_copyHeaderItem(buf, sizeof buf, &head, "ITEM");
    assert(strcmp(buf, "CLDFRC") == 0);
    GT3_copyHeaderItem(buf, sizeof buf, &head, "AITM3");
    assert(strcmp(buf, "ISCCPTP49") == 0);
}


int
main(int argc, char **argv)
{
    open_logging(stderr, "editheader");
    set_logging_level("verbose");
    test1();
    return 0;
}
#endif
