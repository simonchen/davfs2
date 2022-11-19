#include "util.h"

void ERR_AT_LINE(const char* filename, int lineno, const char *fmt, ...)
{
    va_list _args;
    va_start(_args, fmt);
    printf("%s:%d\n", filename, lineno);
    vprintf(fmt, _args);
    va_end(_args);
}
