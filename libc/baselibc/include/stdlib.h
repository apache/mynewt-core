/*
 * stdlib.h
 */

#ifndef _STDLIB_H
#define _STDLIB_H

#include <klibc/extern.h>
#include <klibc/inline.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

__extern_inline int abs(int __n)
{
	return (__n < 0) ? -__n : __n;
}

__extern int atoi(const char *);
__extern long atol(const char *);
__extern long long atoll(const char *);

__extern double atof(const char *str);
__extern double strtod(const char *nptr, char **endptr);

__extern_inline long labs(long __n)
{
	return (__n < 0L) ? -__n : __n;
}

__extern_inline long long llabs(long long __n)
{
	return (__n < 0LL) ? -__n : __n;
}

__extern void free(void *);
__extern void *malloc(size_t);
__extern void *calloc(size_t, size_t);
__extern void *realloc(void *, size_t);

/* Giving malloc some memory from which to allocate */
__extern void add_malloc_block(void *, size_t);
__extern void get_malloc_memory_status(size_t *, size_t *);

/* Malloc locking
 * Until the callbacks are set, malloc doesn't do any locking.
 * malloc_lock() *may* timeout, in which case malloc() will return NULL.
 */
typedef bool (*malloc_lock_t)();
typedef void (*malloc_unlock_t)();
__extern void set_malloc_locking(malloc_lock_t, malloc_unlock_t);

__extern long strtol(const char *, char **, int);
__extern long long strtoll(const char *, char **, int);
__extern unsigned long strtoul(const char *, char **, int);
__extern unsigned long long strtoull(const char *, char **, int);

typedef int (*__comparefunc_t) (const void *, const void *);
__extern void *bsearch(const void *, const void *, size_t, size_t,
		       __comparefunc_t);
__extern void qsort(void *, size_t, size_t, __comparefunc_t);

__extern long jrand48(unsigned short *);
__extern long mrand48(void);
__extern long nrand48(unsigned short *);
__extern long lrand48(void);
__extern unsigned short *seed48(const unsigned short *);
__extern void srand48(long);

__extern_inline char *getenv(const char *name)
{
	return NULL;
}

#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1
__extern void _exit(int s);
__extern_inline void exit(int err)
{
	_exit(err);
}

#define RAND_MAX 0x7fffffff
__extern_inline int rand(void)
{
	return (int)lrand48();
}
__extern_inline void srand(unsigned int __s)
{
	srand48(__s);
}
__extern_inline long random(void)
{
	return lrand48();
}
__extern_inline void srandom(unsigned int __s)
{
	srand48(__s);
}

#ifdef __cplusplus
}
#endif

#endif				/* _STDLIB_H */
