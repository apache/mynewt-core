//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A coredump storage implementation that uses the active OTA staging area in the partition
//! area for storing crash information. If a ota_* slot is not in the partitions*.csv file, no
//! coredump will be saved.

#include "sdkconfig.h"

#if CONFIG_MEMFAULT_COREDUMP_STORAGE_FLASH

#include "memfault/panics/coredump.h"
#include "memfault/panics/platform/coredump.h"

#include <stdio.h>

#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"
#include "memfault/esp8266_port/core.h"
#include "memfault/util/crc16_ccitt.h"

#include <esp_ota_ops.h>
#include "rom/ets_sys.h"

#define MEMFAULT_COREDUMP_PART_INIT_MAGIC 0x45524f43

typedef struct {
  uint32_t magic;
  esp_partition_t partition;
  uint32_t crc;
} sEspIdfCoredumpPartitionInfo;

static sEspIdfCoredumpPartitionInfo s_coredump_partition_info;

static uint32_t prv_get_partition_info_crc(void) {
  return memfault_crc16_ccitt_compute(MEMFAULT_CRC16_CCITT_INITIAL_VALUE,
                                      &s_coredump_partition_info,
                                      offsetof(sEspIdfCoredumpPartitionInfo, crc));
}

void memfault_esp_port_coredump_storage_boot(void) {
  const esp_partition_t *core_part = esp_ota_get_next_update_partition(NULL);

  if (core_part == NULL) {
    MEMFAULT_LOG_ERROR("Coredumps enabled but no storage partition found!");
    return;
  }

  MEMFAULT_LOG_INFO("Coredumps will be saved at 0x%x (%dB)",
                    core_part->address, core_part->size);

  // NB: Since we will be accessing this once the system has crashed, we
  // CRC it just to be extra sure the data is valid and has not been
  // corrupted
  s_coredump_partition_info = (sEspIdfCoredumpPartitionInfo) {
    .magic = MEMFAULT_COREDUMP_PART_INIT_MAGIC,
    .partition = *core_part,
  };
  s_coredump_partition_info.crc = prv_get_partition_info_crc();

  // Log an error if there is not enough storage to save the regions currently
  // being tracked
  memfault_coredump_storage_check_size();
}


static const esp_partition_t *prv_get_core_partition(void) {
  if (s_coredump_partition_info.magic != MEMFAULT_COREDUMP_PART_INIT_MAGIC) {
    return NULL;
  }

  return &s_coredump_partition_info.partition;
}

const esp_partition_t *prv_validate_and_get_core_partition(void) {
  const uint32_t crc = prv_get_partition_info_crc();
  if (crc != s_coredump_partition_info.crc) {
    return NULL;
  }

  return prv_get_core_partition();
}

void memfault_platform_coredump_storage_clear(void) {
  const esp_partition_t *core_part = prv_get_core_partition();
  if (core_part == NULL) {
    return;
  }

  const uint32_t invalidate = 0x0;
  if (core_part->size < sizeof(invalidate)) {
    return;
  }
  const esp_err_t err = spi_flash_write(core_part->address, &invalidate,
                                                     sizeof(invalidate));
  if (err != ESP_OK) {
    MEMFAULT_LOG_ERROR("Failed to write data to flash (%d)!", err);
  }
}

void memfault_platform_coredump_storage_get_info(sMfltCoredumpStorageInfo *info) {
  // we are about to perform a sequence of operations on coredump storage
  // sanity check that the memory holding the info is populated and not corrupted
  const esp_partition_t *core_part = prv_validate_and_get_core_partition();
  if (core_part == NULL) {
    MEMFAULT_ESP_PANIC_PRINTF("No valid coredump storage region found\r\n!");
    *info = (sMfltCoredumpStorageInfo) { 0 };
    return;
  }

  *info  = (sMfltCoredumpStorageInfo) {
    .size = core_part->size,
    .sector_size = SPI_FLASH_SEC_SIZE,
  };
}

bool memfault_platform_coredump_save_begin(void) {
  const esp_partition_t *core_part = prv_validate_and_get_core_partition();
  if (core_part == NULL) {
    return false;
  }

  MEMFAULT_ESP_PANIC_PRINTF("Saving Memfault Coredump!\r\n");
  return true;
}

static bool prv_coredump_storage_write(uint32_t offset, const void *data, size_t data_len) {
  const esp_partition_t *core_part = prv_get_core_partition();
  if (core_part == NULL) {
    return false;
  }

  if ((offset + data_len) > core_part->size) {
    return false;
  }

  const size_t address = core_part->address + offset;
  const esp_err_t err = spi_flash_write(address, data, data_len);
  if (err != ESP_OK) {
    MEMFAULT_ESP_PANIC_PRINTF("coredump write failed: 0x%x %d rv=%d\r\n",
                              (int)address, (int)data_len, (int)err);
  }
  return (err == ESP_OK);
}

bool memfault_platform_coredump_storage_write(uint32_t offset, const void *data, size_t data_len) {

  // Empirically, esp8266 spi_flash_write() routine seems to only support writes up to ~70kB or
  // so. Since we may be copying a large hunk of RAM as part of a coredump we split the write into
  // smaller portions.
  const size_t max_program_length = 4096; // arbitrary
  const uint8_t *datap = data;
  while (data_len > 0) {
    const uint32_t bytes_to_write = MEMFAULT_MIN(max_program_length, data_len);

    if (!prv_coredump_storage_write(offset, datap, bytes_to_write)) {
      return false;
    }

    data_len -= bytes_to_write;
    offset += bytes_to_write;
    datap += bytes_to_write;
  }
  return true;
}

bool memfault_platform_coredump_storage_read(uint32_t offset, void *data, size_t read_len) {
  const esp_partition_t *core_part = prv_get_core_partition();
  if (core_part == NULL) {
    return false;
  }
  if ((offset + read_len) > core_part->size) {
    return false;
  }
  const uint32_t address = core_part->address + offset;
  const esp_err_t err = spi_flash_read(address, data, read_len);
  return (err == ESP_OK);
}

bool memfault_platform_coredump_storage_erase(uint32_t offset, size_t erase_size) {
  MEMFAULT_ESP_PANIC_PRINTF("Erasing Coredump Storage: 0x%x %d\r\n", (int)offset, (int)erase_size);
  const esp_partition_t *core_part = prv_get_core_partition();
  if (core_part == NULL) {
    return false;
  }

  const size_t address = core_part->address + offset;
  const esp_err_t err = spi_flash_erase_range(address, erase_size);
  if (err != ESP_OK) {
    MEMFAULT_ESP_PANIC_PRINTF("coredump erase failed: 0x%x %d\r\n",
                              (int)offset, (int)erase_size);
  } else {
    MEMFAULT_ESP_PANIC_PRINTF("coredump erase complete\r\n");
  }
  return (err == ESP_OK);
}

#endif /* CONFIG_MEMFAULT_COREDUMP_STORAGE_FLASH */
