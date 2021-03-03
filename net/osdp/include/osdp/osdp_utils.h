/*
 * Copyright (c) 2020 Siddharth Chandrasekaran <siddharth@embedjournal.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _OSDP_UTILS_H_
#define _OSDP_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#ifndef NULL
#define NULL                           ((void *)0)
#endif

#ifndef TRUE
#define TRUE                           (1)
#endif

#ifndef FALSE
#define FALSE                          (0)
#endif

#define BYTE_0(x)                      (uint8_t)(((x) >>  0) & 0xFF)
#define BYTE_1(x)                      (uint8_t)(((x) >>  8) & 0xFF)
#define BYTE_2(x)                      (uint8_t)(((x) >> 16) & 0xFF)
#define BYTE_3(x)                      (uint8_t)(((x) >> 24) & 0xFF)

#define ARG_UNUSED(x)                  (void)(x)

#define STR(x) #x
#define XSTR(x) STR(x)

#define MATH_MOD(a, b)                 (((a % b) + b) % b)

#define IS_POW2(n)                     ((n) & ((n) - 1))

#define ARRAY_SIZEOF(x) \
    (sizeof(x) / sizeof(x[0]))

#define ARRAY_BASE(ptr, type, offset) \
    ((char *)(ptr)) - ((sizeof(type)) * offset)

#define OFFSET_OF(type, field) (size_t)(&((type *)(0))->field)

#define OSDP_CONTAINER_OF(ptr, type, field) \
    ((type *)(((char *)(ptr)) - OFFSET_OF(type, field)))

#define U16_STR_SZ (sizeof("65535"))
/*
 #define MAX(a,b) ({ \
        __typeof__ (a) _a = (a); \
        __typeof__ (b) _b = (b); \
        _a > _b ? _a : _b; \
    })

 #define MIN(a,b) ({ \
        __typeof__ (a) _a = (a); \
        __typeof__ (b) _b = (b); \
        _a > _b ? _b : _a; \
    })
 */

/* config_enabled() from the kernel */
#define __IS_ENABLED1(x)             __IS_ENABLED2(__XXXX ## x)
#define __XXXX1                       __YYYY,
#define __IS_ENABLED2(y)             __IS_ENABLED3(y 1, 0)
#define __IS_ENABLED3(_i, val, ...)   val

#define IS_ENABLED(x)                 __IS_ENABLED1(x)

/* gcc attribute shorthands */
#ifndef __fallthrough
#if __GNUC__ >= 7
#define __fallthrough        __attribute__((fallthrough))
#else
#define __fallthrough
#endif
#endif

#ifndef __packed
#define __packed        __attribute__((__packed__))
#endif

/**
 * @brief Dumps an array of bytes in HEX and ASCII formats for debugging. `head`
 * is string that is printed before the actual bytes are dumped.
 *
 * Example:
 *  int len;
 *  uint8_t data[MAX_LEN];
 *  len = get_data_from_somewhere(data, MAX_LEN);
 *  hexdump(data, len, "Data From Somewhere");
 */
void hexdump(const uint8_t *data, size_t len, const char *fmt, ...);

/**
 * @brief      Convert a single character into a hexadecimal nibble.
 *
 * @param c     The character to convert
 * @param x     The address of storage for the converted number.
 *
 *  @return Zero on success or (negative) error code otherwise.
 */
int char2hex(char c, uint8_t *x);

/**
 * @brief      Convert a hexadecimal string into a binary array.
 *
 * @param hex     The hexadecimal string to convert
 * @param hexlen  The length of the hexadecimal string to convert.
 * @param buf     Address of where to store the binary data
 * @param buflen  Size of the storage area for binary data
 *
 * @return     The length of the binary array, or 0 if an error occurred.
 */
size_t hex2bin(const char *hex, size_t hexlen, uint8_t *buf, size_t buflen);

/**
 * @brief      Convert unsigned 16bit number to string.
 *
 * @param num     The unsigned 16bit number to convert.
 * @param str     Input buffer enough for UINT16_MAX including null terminator.
 *
 * @return     Pointer to start of string.
 */
char *u16_to_str(uint16_t num, char *str);

#endif /* _OSDP_UTILS_H_ */
