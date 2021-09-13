//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Dialog DA1468x implementation of the Memfault watchdog API.
//!

#include "memfault/ports/watchdog.h"

#include <stddef.h>
#include <stdint.h>

#include "hw_watchdog.h"
#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"

// The Dialog DA1468x CPU has a simple watchdog implementation that contains an 8-bit counter,
// pause/resume control, and a configurable "last chance" exception capability that routes
// a watchdog timer expiration to NMI instead of RESET. Each tick of the watchdog counter is
// fixed at 10.24ms for a total span of about 2.6s.
//
// Out of reset the CPU configures the watchdog to generate an NMI when the counter reaches
// zero and, if not fed, generates a RESET 16 ticks later (~160ms). This allows us to
// check some notion of system goodness and feed the watchdog or if the check indicates
// a problem reset the CPU. For the simpler case where one watchdog timer interval should
// never expire in normal operation the NMI vector gives us a chance to ensure there is
// enough time to save coredump information to Flash if so configured.
//
// Note that the watchdog can be configured to simply reset when the timer reaches zero
// but that is a one-way setting and cannot be undone without a CPU reset.

// We default to the max count specified in the dg_configWDOG_RESET_VALUE and
// initially use that as the reload value. The user can change this reload
// value at by calling memfault_software_watchdog_update_timeout().
static struct {
  const uint32_t us_per_tick;
  const uint8_t max_count;
  uint8_t reload_value;
} s_watchdog = {
    .us_per_tick = 10240,
    .max_count = dg_configWDOG_RESET_VALUE,
    .reload_value = dg_configWDOG_RESET_VALUE,
};

int memfault_software_watchdog_enable(void) {
  hw_watchdog_unfreeze();
  return 0;
}

int memfault_software_watchdog_disable(void) {
  return hw_watchdog_freeze() ? 0 : -1;
}

int memfault_software_watchdog_feed(void) {
  hw_watchdog_set_pos_val(s_watchdog.reload_value);
  return 0;
}

int memfault_software_watchdog_update_timeout(uint32_t timeout_ms) {
  // 256 ticks at 10240us/tick. Ensure requested time fits or fail.
  const uint32_t timeout_us = 1000 * timeout_ms;
  if (timeout_us > timeout_ms) {
    // Add one to ensure at least requested time after truncation.
    const uint32_t num_ticks = timeout_us / s_watchdog.us_per_tick + 1;

    // Enforce the max limit.
    if (num_ticks <= s_watchdog.max_count) {
      // Remember for subsequent feed calls.
      s_watchdog.reload_value = num_ticks;
      hw_watchdog_set_pos_val(num_ticks);
      return 0;
    }
  }

  return -1;
}
