/*
 * version.c - version of mipconv
 */
static const char *version_ = "mipconv 3 alpha";

char *
mipconv_version(void)
{
    return (char *)version_;
}
