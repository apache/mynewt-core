/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef __MSC_FAT_VIEW_README_H__
#define __MSC_FAT_VIEW_README_H__

#include <stdint.h>
#include <syscfg/syscfg.h>
#include <os/link_tables.h>
#include <stream/stream.h>

/*
 * README.TXT content
 *
 * README.TXT is assembled from entries placed in link table
 * msc_fat_view_readme. Any package can add entries with:
 *
 *   README_TEXT(pri, "text")          - adds line "text"
 *   README_ENTRY(pri, fn, arg)        - fn(out, arg) writes content to stream
 *   README_SYSCFG_STRING(pri, NAME)   - adds line "NAME: <value of syscfg NAME>"
 *   README_SYSCFG_INT(pri, NAME)      - same for integer value (decimal)
 *   README_SYSCFG_HEX(pri, NAME)      - same for integer value (hexadecimal)
 *   README_INT(pri, var)              - adds line "var: <current value of var>"
 *   README_HEX(pri, var)              - same in hexadecimal
 *
 * Entries are sorted by linker using pri as section name suffix.
 * pri is any preprocessing number (00, 01_1, 30), it is compared as string,
 * so numbers should have same number of digits. Order of entries with same
 * pri is not specified, use README_ENTRY() for multi line content.
 * Suggested values:
 *   00     - header
 *   10..19 - versions
 *   50..   - syscfg values, application content
 *   90..   - footer
 *
 * Entry is present only if object file that defines it is linked in.
 * Entries are created only when msc_fat_view/readme package is in the
 * build (MSC_FAT_VIEW_DEFAULT_README), otherwise macros produce nothing.
 */

/**
 * Function that generates part of README.TXT
 *
 * Function is called every time README.TXT size or sector is computed,
 * it must produce same output on each call.
 *
 * @param out - stream to write content to
 * @param arg - argument from table entry
 * @return number of bytes written, negative on error
 */
typedef int (*msc_fat_view_readme_fn_t)(struct out_stream *out, const void *arg);

struct msc_fat_view_readme_entry {
    /** Function that writes entry content */
    msc_fat_view_readme_fn_t fn;
    /** Argument passed to fn */
    const void *arg;
};

LINK_TABLE(struct msc_fat_view_readme_entry, msc_fat_view_readme)

/*
 * Syscfg value check.
 * Adjacent string literals concatenate, anything else is a syntax error.
 * Bitwise or is not allowed for pointers.
 */
#define MSC_FAT_VIEW_README_STR_VAL(v) ("" v)
#define MSC_FAT_VIEW_README_INT_VAL(v) ((v) | 0)

/*
 * Size of integer variable.
 * Build fails (negative array size) if variable is wider than long,
 * bitwise or fails if it is not an integer.
 */
#define MSC_FAT_VIEW_README_VAR_SIZE(var)                                     \
    (sizeof(var) +                                                            \
     0 * sizeof(char[sizeof((var) | 0) && sizeof(var) <= sizeof(long) ? 1 : -1]))

#if MYNEWT_VAL(MSC_FAT_VIEW_README)

struct msc_fat_view_readme_syscfg_str {
    const char *name;
    const char *value;
};

struct msc_fat_view_readme_syscfg_int {
    const char *name;
    long value;
};

struct msc_fat_view_readme_var {
    const char *name;
    const volatile void *ptr;
    uint8_t size;
    uint8_t is_signed;
};

/* Writes arg (const char *) followed by new line */
int msc_fat_view_readme_text(struct out_stream *out, const void *arg);
/* Writes "name: value" for struct msc_fat_view_readme_syscfg_str */
int msc_fat_view_readme_syscfg_str(struct out_stream *out, const void *arg);
/* Writes "name: value" for struct msc_fat_view_readme_syscfg_int */
int msc_fat_view_readme_syscfg_int(struct out_stream *out, const void *arg);
/* Writes "name: 0xvalue" for struct msc_fat_view_readme_syscfg_int */
int msc_fat_view_readme_syscfg_hex(struct out_stream *out, const void *arg);
/* Writes "name: value" for struct msc_fat_view_readme_var */
int msc_fat_view_readme_var_int(struct out_stream *out, const void *arg);
/* Writes "name: 0xvalue" for struct msc_fat_view_readme_var */
int msc_fat_view_readme_var_hex(struct out_stream *out, const void *arg);

#define MSC_FAT_VIEW_CAT_(a, b) a ## b
#define MSC_FAT_VIEW_CAT(a, b) MSC_FAT_VIEW_CAT_(a, b)

/* Explicit alignment prevents compiler from padding entries in table */
#define MSC_FAT_VIEW_README_ELEMENT(pri, _fn, _arg)                           \
    static const struct msc_fat_view_readme_entry                             \
    MSC_FAT_VIEW_CAT(msc_fat_view_readme_, __COUNTER__)                       \
    __attribute__((section(".msc_fat_view_readme." #pri), used,               \
                   aligned(__alignof__(struct msc_fat_view_readme_entry)))) = \
        {                                                                     \
            .fn = (_fn),                                                      \
            .arg = (_arg)                                                     \
        }

/**
 * Add text line to README.TXT
 *
 * @param pri  - entry order
 * @param text - text of the line (new line is added)
 */
#define README_TEXT(pri, text)                                                \
    MSC_FAT_VIEW_README_ELEMENT(pri, msc_fat_view_readme_text, text)

/**
 * Add generated content to README.TXT
 *
 * @param pri - entry order
 * @param fn  - function of type msc_fat_view_readme_fn_t
 * @param arg - argument passed to fn
 */
#define README_ENTRY(pri, fn, arg)                                            \
    MSC_FAT_VIEW_README_ELEMENT(pri, fn, arg)

#define MSC_FAT_VIEW_README_SYSCFG(pri, _fn, type, name, val)                 \
    MSC_FAT_VIEW_README_ELEMENT(pri, _fn, (&(const struct type) { name, val }))

/*
 * README_SYSCFG_xxx paste setting name directly (MYNEWT_VAL_ ## name)
 * so it works even when the same name is also defined as macro
 * (e.g. OS_MAIN_STACK_SIZE).
 */

/**
 * Add line "NAME: value" with string syscfg value to README.TXT
 *
 * Build fails if value is not a string.
 *
 * @param pri  - entry order
 * @param name - syscfg setting name (without MYNEWT_VAL)
 */
#define README_SYSCFG_STRING(pri, name)                                       \
    MSC_FAT_VIEW_README_SYSCFG(pri, msc_fat_view_readme_syscfg_str,           \
                               msc_fat_view_readme_syscfg_str, #name,         \
                               MSC_FAT_VIEW_README_STR_VAL(MYNEWT_VAL_ ## name))

/**
 * Add line "NAME: value" with decimal syscfg value to README.TXT
 *
 * Build fails if value is not an integer.
 *
 * @param pri  - entry order
 * @param name - syscfg setting name (without MYNEWT_VAL)
 */
#define README_SYSCFG_INT(pri, name)                                          \
    MSC_FAT_VIEW_README_SYSCFG(pri, msc_fat_view_readme_syscfg_int,           \
                               msc_fat_view_readme_syscfg_int, #name,         \
                               MSC_FAT_VIEW_README_INT_VAL(MYNEWT_VAL_ ## name))

/**
 * Add line "NAME: 0xvalue" with hexadecimal syscfg value to README.TXT
 *
 * Build fails if value is not an integer.
 *
 * @param pri  - entry order
 * @param name - syscfg setting name (without MYNEWT_VAL)
 */
#define README_SYSCFG_HEX(pri, name)                                          \
    MSC_FAT_VIEW_README_SYSCFG(pri, msc_fat_view_readme_syscfg_hex,           \
                               msc_fat_view_readme_syscfg_int, #name,         \
                               MSC_FAT_VIEW_README_INT_VAL(MYNEWT_VAL_ ## name))

#define MSC_FAT_VIEW_README_VAR(pri, _fn, var)                                \
    MSC_FAT_VIEW_README_ELEMENT(pri, _fn,                                     \
        (&(const struct msc_fat_view_readme_var) {                            \
            #var, &(var), MSC_FAT_VIEW_README_VAR_SIZE(var),                  \
            (__typeof__(var))-1 < 0 }))

/**
 * Add line "var: value" with decimal value of integer variable to README.TXT
 *
 * Variable is read every time README.TXT is generated, so line shows
 * current value. Build fails if var is not an integer variable or is
 * wider than long.
 *
 * @param pri - entry order
 * @param var - integer variable (e.g. SystemCoreClock)
 */
#define README_INT(pri, var)                                                  \
    MSC_FAT_VIEW_README_VAR(pri, msc_fat_view_readme_var_int, var)

/**
 * Add line "var: 0xvalue" with hexadecimal value of integer variable
 * to README.TXT
 *
 * @param pri - entry order
 * @param var - integer variable
 */
#define README_HEX(pri, var)                                                  \
    MSC_FAT_VIEW_README_VAR(pri, msc_fat_view_readme_var_hex, var)

#else

/* fn and arg are referenced so they are not reported as unused */
#define README_TEXT(pri, text) _Static_assert(1, "")
#define README_ENTRY(pri, fn, arg)                                            \
    _Static_assert(sizeof(&(fn)) && sizeof(arg), "")
/* Values are still checked so mismatch is reported in every build */
#define README_SYSCFG_STRING(pri, name)                                       \
    _Static_assert(sizeof(MSC_FAT_VIEW_README_STR_VAL(MYNEWT_VAL_ ## name)), "")
#define README_SYSCFG_INT(pri, name)                                          \
    _Static_assert(sizeof(MSC_FAT_VIEW_README_INT_VAL(MYNEWT_VAL_ ## name)), "")
#define README_SYSCFG_HEX(pri, name)                                          \
    _Static_assert(sizeof(MSC_FAT_VIEW_README_INT_VAL(MYNEWT_VAL_ ## name)), "")
#define README_INT(pri, var)                                                  \
    _Static_assert(MSC_FAT_VIEW_README_VAR_SIZE(var), "")
#define README_HEX(pri, var)                                                  \
    _Static_assert(MSC_FAT_VIEW_README_VAR_SIZE(var), "")

#endif

#endif /* __MSC_FAT_VIEW_README_H__ */
