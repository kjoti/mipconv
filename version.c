/*
 * version.c - version of mipconv
 */
static const char *version_ = "mipconv 1.4.1";

char *
mipconv_version(void)
{
    return (char *)version_;
}
