#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//!
//! Utilities used to generate "compact" logs by replacing the string formatter with an integer
//! "id" at compilation time. This allows arbitrary length string formatters to be stripped from a
//! binary which has several key benefits:
//!  - Less codespace usage (a uint32_t instead of an arbitrary length string)
//!  - Less bandwidth so quicker to transmit and consequently lower power
//!  - Better obfuscation (i.e with formatters stripped, running "strings" on the binary reveals
//!    less)
//!
//!
//! Note: Some of the macros in this file make use of `##` in `, ## __VA_ARGS__` to support calls
//! with empty __VA_ARGS__. While this is a GNU extension, it's widely supported in most other
//! modern compilers (ARM Compiler 5, Clang, iccarm). When using GCC this means you should be
//! compiling with -std=gnu11 (default), -std=gnu99, etc. We check for this support at compile
//! time via a static assert in "memfault/core/preprocessor.h"

#include <stdbool.h>
#include <stdint.h>

#include "memfault/config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MEMFAULT_COMPACT_LOG_ENABLE

#include "memfault/core/compiler.h"
#include "memfault/core/preprocessor.h"

#define MEMFAULT_LOG_ARG_PROMOTED_TO_INT32 0
#define MEMFAULT_LOG_ARG_PROMOTED_TO_INT64 1
#define MEMFAULT_LOG_ARG_PROMOTED_TO_DOUBLE 2
#define MEMFAULT_LOG_ARG_PROMOTED_TO_STR 3

#ifdef __cplusplus


#ifdef MEMFAULT_UNITTEST
  //! Note: For trace_event Memfault CppUTest tests, we pick up this header but do not actually use
  //! the helpers defined so let's silence the warning generated
#else
#  error "Compact logs not yet available when using CPP"
#endif

#else // C Code implementation


//! Preprocessor macro to encode promotion type info about each va_arg in a uint32_t
//!
//! Utilizes the rules around "default argument promotion" (specifically for va_args) defined in
//! the C specification (http://www.iso-9899.info/wiki/The_Standard) to encode information about
//! the width of arguments. (For more details see "6.5.2.2 Function calls" in the C11 Spec).
//!
//! In short,
//!  - floats are always promoted to doubles
//!  - any other type < sizeof(int) is promoted to the width of an int
//!
//! NOTE 1: We use a special type for "strings" (i.e char *) so we know to encode the value
//! pointed to (the actual NUL terminated string) in this situation rather than the pointer itself.
//!
//! NOTE 2: We use "(arg) + 0" in _Generic() below to explicitly force implicit type conversion on
//! char[N] to (char *) and bitfields. There was an ambiguity in the spec when the feature was
//! introduced in C11 about how arrays should be handled. This has since been fixed but not all
//! compilers (i.e IAR) have picked that up. See notes at:
//!   https://en.cppreference.com/w/c/language/generic
//!
//! NOTE 3: While _Generic was formally added as part of C11, it has been backported as an
//! extension to many older C standard implementations in compilers so we don't explicitly check
//! for C11.

#ifdef __clang__
  // Clangs "Wsizeof-array-decay" generates a false positive with our usage of sizeof()
  // inside _Generic so just disable it when the file is used
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wsizeof-array-decay"
#endif

#define _MEMFAULT_LOG_ARG_PROMOTION_TYPE(arg)                           \
  _Generic((arg) + 0,                                                   \
           float:  MEMFAULT_LOG_ARG_PROMOTED_TO_DOUBLE,                 \
           double:  MEMFAULT_LOG_ARG_PROMOTED_TO_DOUBLE,                \
           long double:  MEMFAULT_LOG_ARG_PROMOTED_TO_DOUBLE,           \
           char*:  MEMFAULT_LOG_ARG_PROMOTED_TO_STR,                    \
           const char*:  MEMFAULT_LOG_ARG_PROMOTED_TO_STR,              \
           default: sizeof((arg) + 0) <= sizeof(int) ?                  \
              MEMFAULT_LOG_ARG_PROMOTED_TO_INT32 : MEMFAULT_LOG_ARG_PROMOTED_TO_INT64) \

#endif

#define _MF_FMT_0(fmt_op)          0
#define _MF_FMT_1(fmt_op, X)       fmt_op(X, 0)
#define _MF_FMT_2(fmt_op, X, ...)  fmt_op(X, 2)  | _MF_FMT_1(fmt_op, __VA_ARGS__)
#define _MF_FMT_3(fmt_op, X, ...)  fmt_op(X, 4)  | _MF_FMT_2(fmt_op, __VA_ARGS__)
#define _MF_FMT_4(fmt_op, X, ...)  fmt_op(X, 6)  | _MF_FMT_3(fmt_op, __VA_ARGS__)
#define _MF_FMT_5(fmt_op, X, ...)  fmt_op(X, 8)  | _MF_FMT_4(fmt_op, __VA_ARGS__)
#define _MF_FMT_6(fmt_op, X, ...)  fmt_op(X, 10) | _MF_FMT_5(fmt_op, __VA_ARGS__)
#define _MF_FMT_7(fmt_op, X, ...)  fmt_op(X, 12) | _MF_FMT_6(fmt_op, __VA_ARGS__)
#define _MF_FMT_8(fmt_op, X, ...)  fmt_op(X, 14) | _MF_FMT_7(fmt_op, __VA_ARGS__)
#define _MF_FMT_9(fmt_op, X, ...)  fmt_op(X, 16) | _MF_FMT_8(fmt_op, __VA_ARGS__)
#define _MF_FMT_10(fmt_op, X, ...) fmt_op(X, 18) | _MF_FMT_9(fmt_op, __VA_ARGS__)
#define _MF_FMT_11(fmt_op, X, ...) fmt_op(X, 20) | _MF_FMT_10(fmt_op, __VA_ARGS__)
#define _MF_FMT_12(fmt_op, X, ...) fmt_op(X, 22) | _MF_FMT_11(fmt_op, __VA_ARGS__)
#define _MF_FMT_13(fmt_op, X, ...) fmt_op(X, 24) | _MF_FMT_12(fmt_op, __VA_ARGS__)
#define _MF_FMT_14(fmt_op, X, ...) fmt_op(X, 26) | _MF_FMT_13(fmt_op, __VA_ARGS__)
#define _MF_FMT_15(fmt_op, X, ...) fmt_op(X, 28) | _MF_FMT_14(fmt_op, __VA_ARGS__)

#define _MF_GET_MACRO(_0,_1,_2,_3,_4,_5,_6,_7, _8, _9, _10, _11, _12, _13, _14, _15, NAME,...) NAME
#define _MF_FOR_EACH(action, ...)                                                  \
  _MF_GET_MACRO(_0, ## __VA_ARGS__, _MF_FMT_15, _MF_FMT_14, _MF_FMT_13, _MF_FMT_12, _MF_FMT_11, \
                _MF_FMT_10, _MF_FMT_9, _MF_FMT_8, _MF_FMT_7, _MF_FMT_6, _MF_FMT_5,              \
                _MF_FMT_4, _MF_FMT_3, _MF_FMT_2, _MF_FMT_1, _MF_FMT_0)                          \
  (action, ## __VA_ARGS__)

#define MEMFAULT_FMT_ENCODE_OP(_arg, _shift) (_MEMFAULT_LOG_ARG_PROMOTION_TYPE(_arg) << _shift)

//! Generates a compressed representation of the va_arg format (up to 15 arguments)
//!
//! The value itself resolves at compilation time so there is 0 overhead at runtime
//!
//! Encoding scheme:
//!   - Bits following the most significant bit set to 1 are in use
//!   - 2 bits per argument encode the promotion type. For arg0 - argN, most significant 2 bits in
//!     use correspond to argument 0 format, least significant 2 bits correspond to argument N
//!     format
//!
//! Examples:
//! 0b0000.0001 => 0 arguments
//! 0b0000.0110 => 1 argument  (arg0=0b10=double)
//! 0b0111.0100 => 3 arguments (arg0=0b11=str, arg1=0b01=int64, arg2=0b00=int)
#define MFLT_GET_COMPRESSED_LOG_FMT(...)                    \
  ((_MF_FOR_EACH(MEMFAULT_FMT_ENCODE_OP, ## __VA_ARGS__)) | \
   (1 << (2 * MEMFAULT_ARG_COUNT_UP_TO_32(__VA_ARGS__))))


//! The "special" ELF section all compact log format information is placed in
//!
//! Since data in this section is _never_ read by the firmware, it does not need
//! to be placed in the binary flashed on device.
//!
//! With GNU GCC's ld format, the "INFO" attribute can be used to mark a section as
//! non-allocated. The following addition will need to be made to the .ld script:
//!
//!    log_fmt 0xF0000000 (INFO):
//!    {
//!       KEEP(*(*.log_fmt_hdr))
//!       KEEP(*(log_fmt))
//!    }
#define MEMFAULT_LOG_FMT_ELF_SECTION MEMFAULT_PUT_IN_SECTION("log_fmt")

//! The metadata we track in the ELF for each compact log in the code.
//! This is used to recover the original information
#define MEMFAULT_LOG_FMT_ELF_SECTION_ENTRY(format, ...)                      \
  MEMFAULT_LOG_FMT_ELF_SECTION static const char _memfault_log_fmt_ptr[] =   \
      MEMFAULT_EXPAND_AND_QUOTE(MEMFAULT_ARG_COUNT_UP_TO_32(__VA_ARGS__))";" \
      __FILE__ ";" MEMFAULT_EXPAND_AND_QUOTE(__LINE__) ";" format


#define MEMFAULT_LOG_FMT_ELF_SECTION_ENTRY_PTR \
  ((uint32_t)(uintptr_t)&_memfault_log_fmt_ptr[0])


typedef struct MemfaultLogFmtElfSectionHeader {
  uint32_t magic;
  uint8_t version;
  uint8_t rsvd[3];
} sMemfaultLogFmtElfSectionHeader;

extern const sMemfaultLogFmtElfSectionHeader g_memfault_log_fmt_elf_section_hdr;

#endif /* MEMFAULT_COMPACT_LOG_ENABLE */

#ifdef __cplusplus
}
#endif
