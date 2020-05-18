/*
 * strntoimax.h
 */

#ifndef _STRNTOIMAX_H
#define _STRNTOIMAX_H

#include <klibc/extern.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

__extern intmax_t strntoimax(const char *, char **, int, size_t);
__extern uintmax_t strntoumax(const char *, char **, int, size_t);

#ifdef __cplusplus
}
#endif

#endif /* _STRNTOIMAX_H */
