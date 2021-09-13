//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include "memfault/components.h"
#include "memfault/ports/reboot_reason.h"

#include "datasheet.h"

#if !defined(MEMFAULT_EVENT_STORAGE_RAM_SIZE)
#if defined (__DA14531__)
#define MEMFAULT_EVENT_STORAGE_RAM_SIZE 128
#else
#define MEMFAULT_EVENT_STORAGE_RAM_SIZE 512
#endif
#endif

int memfault_platform_boot(void) {
  memfault_platform_reboot_tracking_boot();
  memfault_build_info_dump();
  memfault_device_info_dump();

  static uint8_t s_event_storage[MEMFAULT_EVENT_STORAGE_RAM_SIZE];
  const sMemfaultEventStorageImpl *evt_storage =
      memfault_events_storage_boot(s_event_storage, sizeof(s_event_storage));
  memfault_trace_event_boot(evt_storage);

  memfault_reboot_tracking_collect_reset_info(evt_storage);

  sMemfaultMetricBootInfo boot_info = {
    .unexpected_reboot_count = memfault_reboot_tracking_get_crash_count(),
  };
  memfault_metrics_boot(evt_storage, &boot_info);

  return 0;
}

MEMFAULT_NORETURN void memfault_platform_reboot(void) {
  const uint16_t tmp = (GetWord16(SYS_CTRL_REG) & ~REMAP_ADR0) | 0 | SW_RESET;
  SetWord16(SYS_CTRL_REG, tmp);

  MEMFAULT_UNREACHABLE;
}
