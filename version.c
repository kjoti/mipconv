/*
 * version.c - version of mipconv
 */
#include "internal.h"

static const char *version_ = "mipconv 0.1";

char *
mipconv_version(void)
{
    return (char *)version_;
}
