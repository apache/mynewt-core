#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! A subsystem which can (optionally) be used to trace _all_ reboots taking place on the system
//!
//! The Memfault "panics" component will automatically save coredumps anytime the system crashes.
//! However, it can sometimes be useful to track other types of reset reasons such as a software
//! initiated reset to complete an OTA, a brown out reset, a hardware watchdog reset, etc
//!
//! To track these types of resets, the "panics" SDK component also exposes a lightweight "reboot
//! tracking" module.  More details can be found in the function descriptions below or a
//! step-by-step setup tutorial is available at https://mflt.io/2QlOlgH
//!
//! A user may also (optionally) use two APIs for catching & reacting to reboot loops:
//!  memfault_reboot_tracking_reset_crash_count()
//!  memfault_reboot_tracking_get_crash_count()

#include <inttypes.h>
#include <stddef.h>

#include "memfault/core/event_storage.h"
#include "memfault/core/reboot_reason_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//! Additional information that can optionally be collected at bootup and appended to the current
//! reset information
typedef struct BootupInfo {
  //! Most MCUs have an always-on register that will report why the device rebooted (i.e normal
  //! reset, brown out reset, watchdog, etc). This value can be provided here to attach the current
  //! value of the register to the reboot information or be 0 otherwise
  uint32_t reset_reason_reg;

  //! If the reason for the current reboot is not already tracked, this value will be used.
  //!
  //! @note This can useful in situations where no code executes from the main application prior to
  //! reboot (for example, a reset button is pressed or another MCU pulls power to the rail) but
  //! info is made available to the application after bootup as to why it was reset (i.e bootloader
  //! passes additional state about reset to main app).
  //!
  //! @note If there is not additional info available about the reset, this should be set to 0
  //! (kMfltRebootReason_Unknown).
  eMemfaultRebootReason reset_reason;
} sResetBootupInfo;

#define MEMFAULT_REBOOT_TRACKING_REGION_SIZE 64

//! Sets the memory region used for reboot tracking.
//!
//! @note This region should _not_ initialized by your bootloader or application.
//!
//!    To achieve this behavior, some compilers have NOINIT directives or with GCC LD
//!    this behavior can be easily achieved with something like:
//!
//!    // In a C File
//!    #include "memfault/core/compiler.h"
//!    MEMFAULT_PUT_IN_SECTION(".mflt_reboot_info")
//!    static uint8_t s_reboot_tracking[MEMFAULT_REBOOT_TRACKING_REGION_SIZE];
//!
//!    // In device LD file
//!    NOINIT (rw) :  ORIGIN = <addr>, LENGTH = 0x20
//!    .noinit (NOLOAD): { KEEP(*(*.mflt_reboot_info)) } > NOINIT
//!
//! @note The size of the region should be MEMFAULT_REBOOT_TRACKING_REGION_SIZE
//! @note This should be called once on bootup of the system prior to making any other
//!   reboot_tracking calls
//! @param start_addr The location where reboot tracking is located
//! @param bootup_info See struct for more details. Can be NULL if there is no info
//!  to provide
void memfault_reboot_tracking_boot(void *start_addr, const sResetBootupInfo *bootup_info);

typedef struct MfltRebootTrackingRegInfo {
  uint32_t pc;
  uint32_t lr;
} sMfltRebootTrackingRegInfo;

//! Flag that a reboot is about to take place
//!
//! This is automatically called by the Memfault API for fault handlers and when
//! memfault_fault_handling_assert() is invoked
//!
//! It can also be called for happy-path reboots such as a reboot due to a user clicking
//! a button or a reboot due to an OTA update taking place. It's up to the user of the SDK
//! to call the API in these scenarios
//! @param reboot_reason The reason for the reboot. See eMemfaultRebootReason for options
//! @param reg Register state at the time the reboot was initiated or NULL if no state is available
void memfault_reboot_tracking_mark_reset_imminent(eMemfaultRebootReason reboot_reason,
                                                  const sMfltRebootTrackingRegInfo *reg);

//! Collects recent reset info and pushes it to memfault_event_storage so that the data can
//! can be sent out using the Memfault data packetizer
//!
//! @param storage_impl The event storage implementation being used (returned from
//!   memfault_events_storage_boot())
//! @return 0 on success or if there was nothing to collect, error code otherwise
int memfault_reboot_tracking_collect_reset_info(const sMemfaultEventStorageImpl *storage_impl);

//! Compute the worst case number of bytes required to serialize Memfault data
//!
//! @return the worst case amount of space needed to serialize an event
size_t memfault_reboot_tracking_compute_worst_case_storage_size(void);

//! Get the current crash count
//!
//! Every time the device resets due to a Reason of Unknown or Error, the crash count
//! is incremented.  A user of the SDK may (optionally) use this information to determine
//! if the device is crash looping and if so take recovery action.
//!
//! @return crash count
size_t memfault_reboot_tracking_get_crash_count(void);

//! Reset the crash count to 0
void memfault_reboot_tracking_reset_crash_count(void);

//! Flags that a coredump has been collected as part of this reboot
//!
//! @note This is called by the "panics" component coredump integration automatically and should
//! never need to be called by an end user directly
void memfault_reboot_tracking_mark_coredump_saved(void);


#ifdef __cplusplus
}
#endif
