//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include "sdkconfig.h"

#include <string.h>

#include "memfault/core/platform/core.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault/core/trace_event.h"
#include "memfault/metrics/metrics.h"
#include "memfault/ports/reboot_reason.h"

#if CONFIG_MEMFAULT_EVENT_COLLECTION_ENABLED

static uint8_t s_event_storage[CONFIG_MEMFAULT_EVENT_STORAGE_RAM_SIZE];

// Note: The ESP8266 noinit region appears to overlap with the heap in the bootloader so this
// region may actually get cleared across reset. In this scenario, we will still have the reset
// info from the RTC backup domain to work with.
static MEMFAULT_PUT_IN_SECTION(".noinit.reboot_info")
    uint8_t s_reboot_tracking[MEMFAULT_REBOOT_TRACKING_REGION_SIZE];

void memfault_esp_port_event_collection_boot(void) {

  sResetBootupInfo reset_info;
  memfault_reboot_reason_get(&reset_info);
  memfault_reboot_tracking_boot(s_reboot_tracking, &reset_info);

  const sMemfaultEventStorageImpl *evt_storage =
      memfault_events_storage_boot(s_event_storage, sizeof(s_event_storage));
  memfault_trace_event_boot(evt_storage);
  memfault_reboot_tracking_collect_reset_info(evt_storage);


#if CONFIG_MEMFAULT_EVENT_HEARTBEATS_ENABLED
  sMemfaultMetricBootInfo boot_info = {
    .unexpected_reboot_count = memfault_reboot_tracking_get_crash_count(),
  };
  memfault_metrics_boot(evt_storage, &boot_info);
#endif
}

#endif /* MEMFAULT_EVENT_COLLECTION_ENABLED */
