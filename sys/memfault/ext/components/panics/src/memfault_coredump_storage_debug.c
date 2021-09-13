//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! A collection of utilities that can be used to validate the platform port of
//! the memfault coredump storage API is working as expected.
//!
//! Example Usage:
//!
//! void validate_coredump_storage_implementation(void) {
//!    // exercise storage routines used during a fault
//!    __disable_irq();
//!    memfault_coredump_storage_debug_test_begin()
//!    __enabled_irq();
//!
//!   // analyze results from test and print results to console
//!   memfault_coredump_storage_debug_test_finish();
//! }

#include "memfault/panics/coredump.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"
#include "memfault/panics/platform/coredump.h"

typedef enum {
  kMemfaultCoredumpStorageTestOp_Prepare = 0,
  kMemfaultCoredumpStorageTestOp_Erase,
  kMemfaultCoredumpStorageTestOp_Write,
  kMemfaultCoredumpStorageTestOp_Clear,
  kMemfaultCoredumpStorageTestOp_GetInfo,
} eMemfaultCoredumpStorageTestOp;

typedef enum {
  kMemfaultCoredumpStorageResult_Success = 0,
  kMemfaultCoredumpStorageResult_PlatformApiFail,
  kMemfaultCoredumpStorageResult_ReadFailed,
  kMemfaultCoredumpStorageResult_CompareFailed,
} eMemfaultCoredumpStorageResult;

typedef struct {
  eMemfaultCoredumpStorageResult result;
  eMemfaultCoredumpStorageTestOp op;
  uint32_t offset;
  const uint8_t *expected_buf;
  uint32_t size;
} sMemfaultCoredumpStorageTestResult;

static sMemfaultCoredumpStorageTestResult s_test_result;
static uint8_t s_read_buf[16];

static void prv_record_failure(
    eMemfaultCoredumpStorageTestOp op, eMemfaultCoredumpStorageResult result,
    uint32_t offset, uint32_t size) {
  s_test_result = (sMemfaultCoredumpStorageTestResult) {
    .op = op,
    .result = result,
    .offset = offset,
    .size = size,
  };
}

//! Helper to scrub the read buffer with a pattern not used by the test
//! This way even if a call to memfault_platform_coredump_storage_read()
//! returns true but does not populate the buffer we will not have a match
static void prv_scrub_read_buf(void) {
  const uint8_t unused_pattern = 0xef;
  memset(s_read_buf, unused_pattern, sizeof(s_read_buf));
}

static bool prv_verify_erased(uint8_t byte) {
  // NB: Depending on storage topology, pattern can differ
  //   0x00 if coredump storage is in RAM
  //   0xFF if coredump storage is some type of flash (i.e NOR, EMMC, etc)
  return ((byte == 0x00) || (byte == 0xff));
}

bool memfault_coredump_storage_debug_test_begin(void) {
  sMfltCoredumpStorageInfo info = { 0 };
  memfault_platform_coredump_storage_get_info(&info);
  if (info.size == 0) {
    prv_record_failure(kMemfaultCoredumpStorageTestOp_GetInfo,
                       kMemfaultCoredumpStorageResult_PlatformApiFail,
                       0, info.size);
    return false;
  }

  // On some ports there maybe some extra setup that needs to occur
  // before we can safely use the backing store without interrupts
  // enabled. Call this setup function now.
  if (!memfault_platform_coredump_save_begin()) {
    prv_record_failure(kMemfaultCoredumpStorageTestOp_Prepare,
                       kMemfaultCoredumpStorageResult_PlatformApiFail,
                       0, info.size);
    return false;
  }

  //
  // Confirm we can erase the storage region
  //

  if (!memfault_platform_coredump_storage_erase(0, info.size)) {
    prv_record_failure(kMemfaultCoredumpStorageTestOp_Erase,
                       kMemfaultCoredumpStorageResult_PlatformApiFail,
                       0, info.size);
    return false;
  }

  for (size_t i = 0; i < info.size; i+= sizeof(s_read_buf)) {
    prv_scrub_read_buf();

    const size_t bytes_to_read = MEMFAULT_MIN(sizeof(s_read_buf), info.size - i);
    if (!memfault_platform_coredump_storage_read(i, s_read_buf, bytes_to_read)) {
      prv_record_failure(kMemfaultCoredumpStorageTestOp_Erase,
                         kMemfaultCoredumpStorageResult_ReadFailed, i, bytes_to_read);
      return false;
    }

    for (size_t j = 0; j <  bytes_to_read; j++) {
      if (!prv_verify_erased(s_read_buf[j])) {
        prv_record_failure(kMemfaultCoredumpStorageTestOp_Erase,
                           kMemfaultCoredumpStorageResult_CompareFailed, j + i, 1);
        return false;
      }
    }
  }

  //
  // Confirm we can write to storage by alternating writes of a 12 byte & 7 byte pattern.
  // This way we can verify that writes starting at different offsets are working
  //
  // The data for memfault coredumps is always written sequentially with the exception of
  // the 12 byte header which is written last. We will simulate that behavior here.
  //

  static const uint8_t pattern1[] = {
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab };
  MEMFAULT_STATIC_ASSERT(sizeof(pattern1) < sizeof(s_read_buf), "pattern1 is too long");

  static const uint8_t pattern2[] = { 0x5f, 0x5e, 0x5d, 0x5c, 0x5b, 0x5a, 0x59};
  MEMFAULT_STATIC_ASSERT(sizeof(pattern2) < sizeof(s_read_buf), "pattern2 is too long");

  struct {
    const uint8_t *pattern;
    size_t len;
  } patterns[] = {
    {
      .pattern = pattern1,
      .len = sizeof(pattern1)
    },
    {
      .pattern = pattern2,
      .len = sizeof(pattern2)
    },
  };

  #define MEMFAULT_COREDUMP_STORAGE_HEADER_LEN 12
  MEMFAULT_STATIC_ASSERT(sizeof(pattern1) == MEMFAULT_COREDUMP_STORAGE_HEADER_LEN,
                         "pattern1 length must match coredump header size");

  // Skip the "header" and begin writing alternating patterns to various offsets
  size_t offset = sizeof(pattern1);
  size_t num_writes = 1;
  while (offset < info.size) {
    const size_t pattern_idx = num_writes % MEMFAULT_ARRAY_SIZE(patterns);
    num_writes++;

    const uint8_t *pattern = patterns[pattern_idx].pattern;
    size_t pattern_len = patterns[pattern_idx].len;
    pattern_len = MEMFAULT_MIN(pattern_len, info.size - offset);

    if (!memfault_platform_coredump_storage_write(offset, pattern, pattern_len)) {
      prv_record_failure(kMemfaultCoredumpStorageTestOp_Write,
                         kMemfaultCoredumpStorageResult_PlatformApiFail, offset, pattern_len);
      return false;
    }
    offset += pattern_len;
  }

  // now simulate writing a coredump header
  if (!memfault_platform_coredump_storage_write(0, pattern1, sizeof(pattern1))) {
    prv_record_failure(kMemfaultCoredumpStorageTestOp_Write,
                       kMemfaultCoredumpStorageResult_PlatformApiFail, offset, sizeof(pattern1));
    return false;
  }

  // now read back what we wrote and confirm it matches the expected pattern sequence
  offset = 0;
  num_writes = 0;
  while (offset < info.size) {
    const size_t pattern_idx = num_writes % MEMFAULT_ARRAY_SIZE(patterns);
    num_writes++;

    const uint8_t *pattern = patterns[pattern_idx].pattern;
    size_t pattern_len = patterns[pattern_idx].len;
    pattern_len = MEMFAULT_MIN(pattern_len, info.size - offset);

    prv_scrub_read_buf();

    if (!memfault_platform_coredump_storage_read(offset, s_read_buf, pattern_len)) {
      prv_record_failure(kMemfaultCoredumpStorageTestOp_Write,
                         kMemfaultCoredumpStorageResult_ReadFailed, offset, pattern_len);
      return false;
    }

    if (memcmp(s_read_buf, pattern, pattern_len) != 0) {
      prv_record_failure(kMemfaultCoredumpStorageTestOp_Write,
                         kMemfaultCoredumpStorageResult_CompareFailed, offset, pattern_len);
      s_test_result.expected_buf = pattern;
      return false;
    }

    offset += pattern_len;
  }

  s_test_result = (sMemfaultCoredumpStorageTestResult) {
    .result = kMemfaultCoredumpStorageResult_Success,
  };
  return true;
}

static bool prv_verify_coredump_clear_operation(void) {
  memfault_platform_coredump_storage_clear();

  const size_t min_clear_size = 1;
  prv_scrub_read_buf();

  // NB: memfault_coredump_read() instead of memfault_platform_coredump_read() here because that's
  // the routine we use when the system is running (in case that mode needs locking)
  if (!memfault_coredump_read(0, s_read_buf, min_clear_size)) {
    prv_record_failure(kMemfaultCoredumpStorageTestOp_Clear,
                       kMemfaultCoredumpStorageResult_ReadFailed, 0, min_clear_size);
    return false;
  }

  if (!prv_verify_erased(s_read_buf[0])) {
    prv_record_failure(kMemfaultCoredumpStorageTestOp_Clear,
                       kMemfaultCoredumpStorageResult_CompareFailed, 0, 1);
    return false;
  }

  return true;
}

static void prv_hexdump(const char *prefix, const uint8_t *buf, size_t buf_len) {
  #define MAX_BUF_LEN (sizeof(s_read_buf) * 2 + 1)
  char hex_buffer[MAX_BUF_LEN];
  for (uint32_t j = 0; j < buf_len; ++j) {
    sprintf(&hex_buffer[j * 2], "%02x", buf[j]);
  }
  // make sure buffer is NUL terminated even if buf_len = 0
  hex_buffer[buf_len * 2] = '\0';
  MEMFAULT_LOG_INFO("%s: %s", prefix, hex_buffer);
}

bool memfault_coredump_storage_debug_test_finish(void) {
  if ((s_test_result.result == kMemfaultCoredumpStorageResult_Success)
      && prv_verify_coredump_clear_operation()) {
    MEMFAULT_LOG_INFO("Coredump Storage Verification Passed");
    return true;
  }

  MEMFAULT_LOG_INFO("Coredump Storage Verification Failed");

  const char *op_suffix;
  switch (s_test_result.op) {
    case kMemfaultCoredumpStorageTestOp_Prepare:
      op_suffix = "prepare";
      break;

    case kMemfaultCoredumpStorageTestOp_Erase:
      op_suffix = "erase";
      break;

    case kMemfaultCoredumpStorageTestOp_Write:
      op_suffix = "write";
      break;

    case kMemfaultCoredumpStorageTestOp_Clear:
      op_suffix = "clear";
      break;

    case kMemfaultCoredumpStorageTestOp_GetInfo:
      op_suffix = "get info";
      break;

    default:
      op_suffix = "unknown";
      break;
  }

  const char *reason_str = "";
  switch (s_test_result.result) {
    case kMemfaultCoredumpStorageResult_Success:
      break;

    case kMemfaultCoredumpStorageResult_PlatformApiFail:
      reason_str = "Api call failed during";
      break;

    case kMemfaultCoredumpStorageResult_ReadFailed:
      reason_str = "Call to memfault_platform_coredump_storage_read() failed during";
      break;

    case kMemfaultCoredumpStorageResult_CompareFailed:
      reason_str = "Read pattern mismatch during";
      break;

    default:
      reason_str = "Unknown";
      break;
  }

  MEMFAULT_LOG_INFO("%s memfault_platform_coredump_storage_%s() test", reason_str, op_suffix);
  MEMFAULT_LOG_INFO("Storage offset: 0x%08"PRIx32", %s size: %d", s_test_result.offset, op_suffix,
                    (int)s_test_result.size);

  if (s_test_result.result == kMemfaultCoredumpStorageResult_CompareFailed) {
    if (s_test_result.expected_buf != NULL) {
      prv_hexdump("Expected", s_test_result.expected_buf, s_test_result.size);
    } else if (s_test_result.op !=  kMemfaultCoredumpStorageTestOp_Write) {
      MEMFAULT_LOG_INFO("expected erase pattern is 0xff or 0x00");
    }

    prv_hexdump("Actual  ", s_read_buf, s_test_result.size);
  }

  return false;
}
