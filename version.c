/*
 * version.c - version of mipconv
 */
#include "internal.h"

static const char *version_ = "mipconv 2010-11-11";

char *
mipconv_version(void)
{
    return (char *)version_;
}
