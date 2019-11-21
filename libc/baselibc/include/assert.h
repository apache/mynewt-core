/*
 * assert.h
 */

#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NDEBUG

/*
 * NDEBUG doesn't just suppress the faulting behavior of assert(),
 * but also all side effects of the assert() argument.  This behavior
 * is required by the C standard, and allows the argument to reference
 * variables that are not defined without NDEBUG.
 */
#define assert(x) ((void)(0))

#else
#include <stddef.h>
#include "os/os_fault.h"

#define assert(x) ((x) ? (void)0 : OS_CRASH())

#endif

#if __STDC_VERSION__ >= 201112L && !defined __cplusplus
#define static_assert _Static_assert
#endif

#ifdef __cplusplus
}
#endif

#endif				/* _ASSERT_H */
