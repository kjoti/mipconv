/*
 * version.c - version of mipconv
 */
static const char *version_ = "mipconv 2.0.0 alpha1 (2018-09-12)";

char *
mipconv_version(void)
{
    return (char *)version_;
}
