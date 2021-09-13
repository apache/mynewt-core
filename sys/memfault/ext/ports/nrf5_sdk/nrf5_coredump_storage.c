//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Implements platform dependency functions required for saving coredumps
//! to internal flash when using the NRF5 SDK.

#include "memfault/panics/coredump.h"

#include <string.h>

#include "app_util.h"
#include "nrf_log.h"
#include "nrf_nvmc.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_soc.h"

#include "memfault/config.h"
#include "memfault/core/compiler.h"
#include "memfault/core/math.h"
#include "memfault/panics/platform/coredump.h"

#ifndef MEMFAULT_COREDUMP_STORAGE_START_ADDR
extern uint32_t __MemfaultCoreStorageStart[];
#define MEMFAULT_COREDUMP_STORAGE_START_ADDR ((uint32_t)__MemfaultCoreStorageStart)
#endif

#ifndef MEMFAULT_COREDUMP_STORAGE_END_ADDR
extern uint32_t __MemfaultCoreStorageEnd[];
#define MEMFAULT_COREDUMP_STORAGE_END_ADDR ((uint32_t)__MemfaultCoreStorageEnd)
#endif

typedef enum {
  kMemfaultCoredumpClearState_Idle = 0,
  kMemfaultCoredumpClearState_ClearRequested,
  kMemfaultCoredumpClearState_ClearInProgress,
} eMemfaultCoredumpClearState;

static eMemfaultCoredumpClearState s_coredump_clear_state;

static void prv_handle_flash_op_complete(bool success) {
  if (s_coredump_clear_state == kMemfaultCoredumpClearState_Idle) {
    return; // not a flash op we care about
  }

  if (s_coredump_clear_state == kMemfaultCoredumpClearState_ClearInProgress &&
      success) {
    // The erase is complete!
    s_coredump_clear_state = kMemfaultCoredumpClearState_Idle;
    return;
  }

  if (!success) {
    NRF_LOG_WARNING("Coredump clear failed, retrying ...");
  }

  // we either haven't kicked off a clear operation yet or our previous attempt
  // was not successful and we need to retry
  memfault_platform_coredump_storage_clear();
}

static void prv_coredump_handle_soc_update(uint32_t sys_evt, void * p_context) {
  switch (sys_evt) {
    case NRF_EVT_FLASH_OPERATION_SUCCESS:
    case NRF_EVT_FLASH_OPERATION_ERROR: {
      const bool success = (sys_evt == NRF_EVT_FLASH_OPERATION_SUCCESS);
      prv_handle_flash_op_complete(success);
      break;
    }

    default:
      break;
  }
}

NRF_SDH_SOC_OBSERVER(m_soc_evt_observer, 0, prv_coredump_handle_soc_update, NULL);

void memfault_platform_coredump_storage_clear(void) {
  // static because sd_flash_write may take place asynchrnously
  const static uint32_t invalidate = 0x0;

  // If the soft device is enabled we need to route our flash operation through it (Code saving
  // backtraces hits the flash directly since the soft device is out of the picture at the time we
  // crash!)

  if (nrf_sdh_is_enabled()) {
    // NB: When the SoftDevice is active, flash operations get scheduled and an asynchronous
    // event is emitted when the operation completes. Therefore, we need to add some tracking
    // to make sure the coredump clear request completes.
    //
    // More details can be found in Section 8 "Flash memory API" of the SoftDevice Spec
    //    https://infocenter.nordicsemi.com/pdf/S140_SDS_v2.1.pdf
    s_coredump_clear_state = kMemfaultCoredumpClearState_ClearRequested;
    const uint32_t rv = sd_flash_write((void *)MEMFAULT_COREDUMP_STORAGE_START_ADDR, &invalidate, 1);
    if (rv == NRF_SUCCESS) {
      s_coredump_clear_state = kMemfaultCoredumpClearState_ClearInProgress;
    } else if (rv == NRF_ERROR_BUSY) {
      // An earlier flash command is still in progress. We will retry when
      // prv_handle_flash_op_complete() is invoked when the in-flight flash op completes.
      NRF_LOG_INFO("Coredump clear deferred, flash busy");
    } else {
      // NB: Any error except for NRF_ERROR_BUSY is indicative of a configuration error of some sort.
      NRF_LOG_ERROR("Unexpected error clearing coredump! %d", rv);
    }
  } else {
    nrf_nvmc_write_word(MEMFAULT_COREDUMP_STORAGE_START_ADDR, invalidate);
  }
}

void memfault_platform_coredump_storage_get_info(sMfltCoredumpStorageInfo *info) {
  const size_t size =
      MEMFAULT_COREDUMP_STORAGE_END_ADDR - MEMFAULT_COREDUMP_STORAGE_START_ADDR;

  *info  = (sMfltCoredumpStorageInfo) {
    .size = size,
    .sector_size = NRF_FICR->CODEPAGESIZE,
  };
}

static bool prv_op_within_flash_bounds(uint32_t offset, size_t data_len) {
  sMfltCoredumpStorageInfo info = { 0 };
  memfault_platform_coredump_storage_get_info(&info);
  return (offset + data_len) <= info.size;
}

//! Don't return any new data while a clear operation is in progress
//!
//! This prevents reading the same coredump again while an erase is in flight.
//!
//! We implement this in memfault_coredump_read() so the logic is only run
//! while the system is running.
bool memfault_coredump_read(uint32_t offset, void *data, size_t read_len) {
  if (s_coredump_clear_state != kMemfaultCoredumpClearState_Idle) {
    // Return false here to indicate that there is no new data to read
    return false;
  }

  return memfault_platform_coredump_storage_read(offset, data, read_len);
}

bool memfault_platform_coredump_storage_write(uint32_t offset, const void *data,
                                              size_t data_len) {
  if (!prv_op_within_flash_bounds(offset, data_len)) {
    return false;
  }

  uint32_t address = MEMFAULT_COREDUMP_STORAGE_START_ADDR + offset;

  const uint32_t word_size = 4;
  if (((address % word_size) == 0) && (((uint32_t)data % word_size) == 0)) {
    const size_t num_words = data_len / word_size;
    nrf_nvmc_write_words(address, data, num_words);
    data_len = data_len % word_size;
    const size_t bytes_written = num_words * word_size;
    address += num_words * word_size;
    const uint8_t *data_rem = data;
    data_rem += bytes_written;
    nrf_nvmc_write_bytes(address, data_rem, data_len);
  } else {
    nrf_nvmc_write_bytes(address, data, data_len);
  }
  return true;
}

bool memfault_platform_coredump_storage_read(uint32_t offset, void *data,
                                             size_t read_len) {
  if (!prv_op_within_flash_bounds(offset, read_len)) {
    return false;
  }

  const uint32_t address = MEMFAULT_COREDUMP_STORAGE_START_ADDR + offset;
  memcpy(data, (void *)address, read_len);
  return true;
}

bool memfault_platform_coredump_storage_erase(uint32_t offset, size_t erase_size) {
  if (!prv_op_within_flash_bounds(offset, erase_size)) {
    return false;
  }

  const size_t sector_size = NRF_FICR->CODEPAGESIZE;

  if ((offset % sector_size) != 0) {
    return false;
  }

  for (size_t sector = offset; sector < erase_size; sector += sector_size) {
    const uint32_t address = MEMFAULT_COREDUMP_STORAGE_START_ADDR + sector;
    nrf_nvmc_page_erase(address);
  }

  return true;
}
