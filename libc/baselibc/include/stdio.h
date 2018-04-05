/*
 * stdio.h
 */

#ifndef _STDIO_H
#define _STDIO_H

#include <klibc/extern.h>
#include <klibc/inline.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The File structure is designed to be compatible with ChibiOS/RT type
 * BaseSequentialStream.
 */
struct File;

typedef struct File FILE;

struct File_methods
{
    size_t (*write)(FILE* instance, const char *bp, size_t n);
    size_t (*read)(FILE* instance, char *bp, size_t n);
};

struct File
{
    const struct File_methods *vmt;
};

#ifndef EOF
# define EOF (-1)
#endif

#ifndef BUFSIZ
# define BUFSIZ 1
#endif

/* Standard file descriptors - implement these globals yourself. */
extern FILE* const stdin;
extern FILE* const stdout;
extern FILE* const stderr;

/* Wrappers around stream write and read */
__extern_inline size_t fread(void *buf, size_t size, size_t nmemb, FILE *stream)
{
    if (stream->vmt->read == NULL) return 0;
    return stream->vmt->read(stream, (char*)buf, size*nmemb) / size;
}

__extern_inline size_t fwrite(const void *buf, size_t size, size_t nmemb, FILE *stream)
{
    if (stream->vmt->write == NULL) return 0;
    return stream->vmt->write(stream, (char*)buf, size*nmemb) / size;
}

__extern_inline int fputs(const char *s, FILE *f)
{
	return fwrite(s, 1, strlen(s), f);
}

__extern_inline int puts(const char *s)
{
	return fwrite(s, 1, strlen(s), stdout) + fwrite("\n", 1, 1, stdout);
}

__extern_inline int fputc(int c, FILE *f)
{
	unsigned char ch = c;
	return fwrite(&ch, 1, 1, f) == 1 ? ch : EOF;
}

__extern char *fgets(char *, int, FILE *);
__extern_inline int fgetc(FILE *f)
{
	unsigned char ch;
	return fread(&ch, 1, 1, f) == 1 ? ch : EOF;
}

__extern int errno;
__extern_inline char *strerror(int errnum)
{
	return (char*)"error_str";
}

#define putc(c,f)  fputc((c),(f))
#define putchar(c) fputc((c),stdout)
#define getc(f) fgetc(f)
#define getchar() fgetc(stdin)

__extern_inline int fflush(FILE *stream)
{
	return 0;
}

__extern int printf(const char *, ...);
__extern int vprintf(const char *, va_list);
__extern int fprintf(FILE *, const char *, ...);
__extern int vfprintf(FILE *, const char *, va_list);
__extern int sprintf(char *, const char *, ...);
__extern int vsprintf(char *, const char *, va_list);
__extern int snprintf(char *, size_t n, const char *, ...);
__extern int vsnprintf(char *, size_t n, const char *, va_list);
__extern int asprintf(char **, const char *, ...);
__extern int vasprintf(char **, const char *, va_list);

__extern int sscanf(const char *, const char *, ...);
__extern int vsscanf(const char *, const char *, va_list);

/* Open a memory buffer for writing.
 Note: Does not write null terminator.*/
struct MemFile
{
    struct File file;
    char *buffer;
    size_t bytes_written;
    size_t size;
};

FILE *fmemopen_w(struct MemFile* storage, char *buffer, size_t size);


#ifdef __cplusplus
}
#endif

#endif				/* _STDIO_H */
