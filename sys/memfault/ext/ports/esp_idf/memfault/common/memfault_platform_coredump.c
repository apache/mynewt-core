//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Port of the Memfault SDK to the esp-idf for esp32 devices

#include "memfault/panics/coredump.h"
#include "memfault/panics/platform/coredump.h"

#include <string.h>
#include <stdint.h>

#include "esp_partition.h"
#include "esp_spi_flash.h"
#include "soc/soc.h"

#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"
#include "memfault/esp_port/spi_flash.h"
#include "memfault/esp_port/uart.h"
#include "memfault/util/crc16_ccitt.h"

// Factor out issues with Espressif's ESP32 to ESP conversion in sdkconfig
#define COREDUMPS_ENABLED \
  (CONFIG_ESP32_ENABLE_COREDUMP || CONFIG_ESP_COREDUMP_ENABLE)
#define COREDUMP_TO_FLASH_ENABLED \
  (CONFIG_ESP32_ENABLE_COREDUMP_TO_FLASH || CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH)

#if !COREDUMPS_ENABLED || !COREDUMP_TO_FLASH_ENABLED
#error "Memfault SDK integration requires CONFIG_ESP32_ENABLE_COREDUMP_TO_FLASH=y sdkconfig setting"
#endif

#define ESP_IDF_COREDUMP_PART_INIT_MAGIC 0x45524f43

// If there is no coredump partition defined or one cannot be defined
// the user can try using an OTA slot instead.
#if CONFIG_MEMFAULT_COREDUMP_USE_OTA_SLOT
#include "esp_ota_ops.h"
#define GET_COREDUMP_PARTITION() \
  esp_ota_get_next_update_partition(NULL);
#else
#define GET_COREDUMP_PARTITION() \
  esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL)
#endif

typedef struct {
  uint32_t magic;
  esp_partition_t partition;
  uint32_t crc;
} sEspIdfCoredumpPartitionInfo;

static sEspIdfCoredumpPartitionInfo s_esp32_coredump_partition_info;

static uint32_t prv_get_partition_info_crc(void) {
  return memfault_crc16_ccitt_compute(MEMFAULT_CRC16_CCITT_INITIAL_VALUE,
                                      &s_esp32_coredump_partition_info,
                                      offsetof(sEspIdfCoredumpPartitionInfo, crc));
}

static const esp_partition_t *prv_get_core_partition(void) {
  if (s_esp32_coredump_partition_info.magic != ESP_IDF_COREDUMP_PART_INIT_MAGIC) {
    return NULL;
  }

  return &s_esp32_coredump_partition_info.partition;
}

//! By default, we attempt to collect all of internal RAM as part of a Coredump
//!
//! @note The function is intentionally defined as weak so someone can
//! easily override the port defaults by re-defining a non-weak version of
//! the function in another file
MEMFAULT_WEAK
const sMfltCoredumpRegion *memfault_platform_coredump_get_regions(
    const sCoredumpCrashInfo *crash_info, size_t *num_regions) {
  static sMfltCoredumpRegion s_coredump_regions[1];

  const uint32_t esp32_dram_start_addr = SOC_DMA_LOW;

  size_t dram_collection_len = SOC_DMA_HIGH - SOC_DMA_LOW;
  const esp_partition_t *core_part = prv_get_core_partition();
  if (core_part != NULL) {
    // NB: Leave some space in storage for other regions collected by the SDK
    dram_collection_len = MEMFAULT_MIN((core_part->size * 7) / 8,
                                       dram_collection_len);
  }

  s_coredump_regions[0] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
      (void *)esp32_dram_start_addr, dram_collection_len);
  *num_regions = MEMFAULT_ARRAY_SIZE(s_coredump_regions);
  return &s_coredump_regions[0];
}

//! Opens partition system on boot to determine where a coredump can be saved
//!
//! @note We override the default implementation using the GNU linkers --wrap feature
//! @note Function invocation is here:
//!   https://github.com/espressif/esp-idf/blob/v4.0/components/esp32/cpu_start.c#L415-L422
void __wrap_esp_core_dump_init(void) {
  const esp_partition_t *const core_part = GET_COREDUMP_PARTITION();

  if (core_part == NULL) {
    MEMFAULT_LOG_ERROR("Coredumps enabled but no partition exists!");
    MEMFAULT_LOG_ERROR("Add \"coredump\" to your partition.csv file");
    return;
  }

  MEMFAULT_LOG_INFO("Coredumps will be saved to '%s' partition",
                    core_part->label);
  MEMFAULT_LOG_INFO("Using entry %p pointing to address 0x%08X",
                    core_part, core_part->address);

  s_esp32_coredump_partition_info = (sEspIdfCoredumpPartitionInfo) {
    .magic = ESP_IDF_COREDUMP_PART_INIT_MAGIC,
    .partition = *core_part,
  };
  s_esp32_coredump_partition_info.crc = prv_get_partition_info_crc();

  // Log an error if there is not enough space to save the information requested
  memfault_coredump_storage_check_size();
}

esp_err_t __wrap_esp_core_dump_image_get(size_t* out_addr, size_t *out_size) {
  if (out_addr == NULL || out_size == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  const esp_partition_t *core_part = prv_get_core_partition();
  if (core_part == NULL) {
    return ESP_FAIL;
  }


  if (!memfault_coredump_has_valid_coredump(out_size)) {
    return ESP_ERR_INVALID_SIZE;
  }

  *out_addr = core_part->address;
  return ESP_OK;
}

const esp_partition_t *prv_validate_and_get_core_partition(void) {
  const uint32_t crc = prv_get_partition_info_crc();
  if (crc != s_esp32_coredump_partition_info.crc) {
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
  const esp_err_t err = memfault_esp_spi_flash_write(core_part->address, &invalidate,
                                                     sizeof(invalidate));
  if (err != ESP_OK) {
    memfault_platform_log(kMemfaultPlatformLogLevel_Error,
                          "Failed to write data to flash (%d)!", err);
  }
}

void memfault_platform_coredump_storage_get_info(sMfltCoredumpStorageInfo *info) {
  // we are about to perform a sequence of operations on coredump storage
  // sanity check that the memory holding the info is populated and not corrupted
  const esp_partition_t *core_part = prv_validate_and_get_core_partition();
  if (core_part == NULL) {
    *info = (sMfltCoredumpStorageInfo) { 0 };
    return;
  }

  *info  = (sMfltCoredumpStorageInfo) {
    .size = core_part->size,
    .sector_size = SPI_FLASH_SEC_SIZE,
  };
}

#if !CONFIG_ESP32_PANIC_SILENT_REBOOT

// barebones printf logic that is safe to run after the esp32 has hit an exception A couple checks
// based on esp-idf version to remain compatible with v3.x and v4.x

static void prv_panic_safe_putchar(char c) {
  // wait for previous byte write to complete
  int i = 0;
  bool ready = false;
  while (i++ < 1000 && !ready) {
    const uint32_t status = READ_PERI_REG(UART_STATUS_REG(MEMFAULT_ESP32_CONSOLE_UART_NUM));
    ready = ((status >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT) >= 126;
  }
  WRITE_PERI_REG(UART_FIFO_REG(MEMFAULT_ESP32_CONSOLE_UART_NUM), c);
}

#else /* !CONFIG_ESP32_PANIC_SILENT_REBOOT */

static void prv_panic_safe_putchar(char c) { }

#endif /* CONFIG_ESP32_PANIC_SILENT_REBOOT */

static void prv_panic_safe_putstr(const char *str) {
  int idx = 0;
  while (str[idx] != 0) {
    prv_panic_safe_putchar(str[idx]);
    idx++;
  }
}

bool memfault_platform_coredump_save_begin(void) {
  prv_panic_safe_putstr("Saving Memfault Coredump!\r\n");
  return (memfault_esp_spi_flash_coredump_begin() == 0);
}

bool memfault_platform_coredump_storage_write(uint32_t offset, const void *data,
                                              size_t data_len) {
  const esp_partition_t *core_part = prv_get_core_partition();
  if (core_part == NULL) {
    return false;
  }

  const size_t address = core_part->address + offset;
  const esp_err_t err = spi_flash_write(address, data, data_len);
  if (err != ESP_OK) {
    prv_panic_safe_putstr("coredump write failed");
  }
  return (err == ESP_OK);
}

bool memfault_platform_coredump_storage_read(uint32_t offset, void *data,
                                             size_t read_len) {
  const esp_partition_t *core_part = prv_get_core_partition();
  if (core_part == NULL) {
    return false;
  }
  if ((offset + read_len) > core_part->size) {
    return false;
  }
  const uint32_t address = core_part->address + offset;
  const esp_err_t err = memfault_esp_spi_flash_read(address, data, read_len);
  return (err == ESP_OK);
}

bool memfault_platform_coredump_storage_erase(uint32_t offset, size_t erase_size) {
  const esp_partition_t *core_part = prv_get_core_partition();
  if (core_part == NULL) {
    return false;
  }

  const size_t address = core_part->address + offset;
  const esp_err_t err = memfault_esp_spi_flash_erase_range(address, erase_size);
  if (err != ESP_OK) {
    prv_panic_safe_putstr("coredump erase failed");
  }
  return (err == ESP_OK);
}
