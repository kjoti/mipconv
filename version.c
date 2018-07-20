/*
 * version.c - version of mipconv
 */
static const char *version_ = "mipconv 2 alpha (2018-07-20)";

char *
mipconv_version(void)
{
    return (char *)version_;
}
