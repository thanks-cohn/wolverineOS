#include <stdio.h>
#include <stdarg.h>
#include "ivault.h"

void ivault_log(const char *level, const char *fmt, ...) {
    va_list args;

    fprintf(stderr, "[IVAULT][%s] ", level ? level : "INFO");

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}
