/*
 * version.c - version of mipconv
 */
static const char *version_ = "mipconv 2.0.0";

char *
mipconv_version(void)
{
    return (char *)version_;
}
