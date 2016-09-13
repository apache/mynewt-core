/*
 * klibc/inline.h
 */

#ifndef _KLIBC_INLINE_H
#define _KLIBC_INLINE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __extern_inline
#define __extern_inline extern inline __attribute__((gnu_inline))
#endif

#ifdef __cplusplus
}
#endif

#endif				/* _KLIBC_INLINE_H */
