#include <stdio.h>

#if defined(linux)
/* Connects the baselibc stdio to normal POSIX stdio */
size_t write(int fd, const void *buf, size_t count);

static size_t stdio_write(FILE *instance, const char *bp, size_t n)
{
    if (instance == stdout)
        return write(1, bp, n);
    else
        return write(2, bp, n);
}
#else
#error No suitable write() implementation.
#endif


static struct File_methods stdio_methods = {
        &stdio_write, NULL
};

static struct File _stdout = {
        &stdio_methods
};

static struct File _stderr = {
        &stdio_methods
};

FILE* const stdout = &_stdout;
FILE* const stderr = &_stderr;

