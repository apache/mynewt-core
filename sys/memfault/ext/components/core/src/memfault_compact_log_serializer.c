//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "memfault/config.h"

#if MEMFAULT_COMPACT_LOG_ENABLE

#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/compact_log_helpers.h"
#include "memfault/core/compact_log_serializer.h"
#include "memfault/util/cbor.h"

//! Compact logs are placed in a linker section named "log_fmt". This is a symbol exposed by the
//! linker which points to the start of the section
extern uint32_t __start_log_fmt;

//! Note: We don't read this in the firmware but it is used during the decode
//! process to sanity check the section is being laid out as we would expect.
MEMFAULT_PUT_IN_SECTION(".log_fmt_hdr")
const sMemfaultLogFmtElfSectionHeader g_memfault_log_fmt_elf_section_hdr = {
  .magic = 0x66474f4c, /* LOGf */
  .version = 1,
};

bool memfault_vlog_compact_serialize(sMemfaultCborEncoder *encoder, uint32_t log_id,
                                     uint32_t compressed_fmt, va_list args) {
  const uint32_t bits_per_arg = 2;
  const uint32_t bits_per_arg_mask =  (1 << bits_per_arg) - 1;

  const size_t num_args = (31UL - MEMFAULT_CLZ(compressed_fmt)) / bits_per_arg;

  if (!memfault_cbor_encode_array_begin(encoder, 1 /* log_id */ + num_args)) {
    return false;
  }

  // We use an offset within the section to reduce the space needed when serializing
  // the log.
  uint32_t log_fmt_offset = log_id - (uint32_t)(uintptr_t)&__start_log_fmt;

  if (!memfault_cbor_encode_unsigned_integer(encoder, log_fmt_offset)) {
    return false;
  }

  for (size_t i = 0; i < num_args; i++) {
    // See memfault/core/compact_log_helpers.h for more details. In essence,
    // the promotion type of a given va_arg is encoded within two bits of the
    // compressed_fmt field.
    const uint32_t type =
        (compressed_fmt >> ((num_args - i - 1) * bits_per_arg)) & bits_per_arg_mask;

    bool success = false;
    switch (type) {

      case MEMFAULT_LOG_ARG_PROMOTED_TO_INT32: {
        int32_t val = va_arg(args, int32_t);
        success = memfault_cbor_encode_signed_integer(encoder, val);
        break;
      }

      case MEMFAULT_LOG_ARG_PROMOTED_TO_INT64: {
        // We differentiate between an 64 bit and 32 bit integer arg
        // by packing the cbor encoded int in an array.
        uint64_t val = va_arg(args, uint64_t);
        success = memfault_cbor_encode_array_begin(encoder, 1) &&
            memfault_cbor_encode_long_signed_integer(encoder, (int64_t)val);
        break;
      }

      case MEMFAULT_LOG_ARG_PROMOTED_TO_DOUBLE: {
        // Note: Per the ARM ABI, doubles are just serialized out onto the stack
        // and occupy 8 bytes:
        //
        // Chapter 7: THE STANDARD VARIANTS
        // For a variadic function the base standard is always used both for argument passing and
        // result return.
        //
        // 6.5 Parameter Passing
        // In the base standard there are no arguments that are candidates for a co-processor
        // register class.
        //
        // So for ARM we could just do the following:
        //  uint64_t val = va_arg(args, uint64_t)
        //
        // But parsing as a double is more portable and get's optimized away by compilers
        // like GCC anyway: https://godbolt.org/z/mVAS7D
        MEMFAULT_STATIC_ASSERT(sizeof(uint64_t) == sizeof(double), "double does not fit in uint64_t");

        // Note: We use memcpy to properly type-pun / avoid strict-aliasing violation:
        //  https://mflt.io/strict-aliasing-type-punning
        double val = va_arg(args, double);
        uint64_t double_as_uint64;
        memcpy(&double_as_uint64, &val, sizeof(val));
        success = memfault_cbor_encode_uint64_as_double(encoder, double_as_uint64);
        break;
      }

      case MEMFAULT_LOG_ARG_PROMOTED_TO_STR: {
        const char *str = va_arg(args, const char*);
        success = memfault_cbor_encode_string(encoder, str != NULL ? str : "(null)");
        break;
      }

      default:
        break;

    }

    if (!success) {
      return false;
    }
  }

  return true;
}

bool memfault_log_compact_serialize(sMemfaultCborEncoder *encoder, uint32_t log_id,
                            uint32_t compressed_fmt,  ...) {
  va_list args;
  va_start(args, compressed_fmt);
  const bool success = memfault_vlog_compact_serialize(encoder, log_id, compressed_fmt, args);
  va_end(args);

  return success;
}

#endif /* MEMFAULT_COMPACT_LOG_ENABLE */
