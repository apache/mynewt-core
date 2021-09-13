//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A Software Watchdog implementation backed by the
//! S32K Low Power Interrupt Timer (LPIT) peripheral & the S32K SDK.
//!
//! More details about the S32K1xx series can be found in the S32K-RM:
//! available from NXPs website
//!
//! By setting MEMFAULT_WATCHDOG_SW_TIMEOUT_SECS to a timeout less than the hardware watchdog, we
//! can guarantee a capture of a coredump when the system is in a wedged state.
//!
//! If your application is using low power modes, consider running the low power timers in
//! with DOZE_EN. This way a hang in stop mode will also be caught.
//!  LPIT0->MSR |= LPIT_MCR_DOZE_EN(1)

#include "memfault/ports/watchdog.h"

#include <inttypes.h>

#include "memfault/config.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"

#include "device_registers.h"

// The Low Power Interrupt Timer Channel to use. The S32K has 4 channels (0-3).
#ifndef MEMFAULT_SOFWARE_WATCHDOG_SOURCE
# define MEMFAULT_SOFWARE_WATCHDOG_SOURCE 0
#endif

// If the LPIT is driven by SIRC or FIRC we can automatically derive the clock frequency. If
// SPLL_CLK or SOS_CLK are used, we need a define to be specified because we can't programmatically
// resolve the external source clock frequency
#ifndef MEMFAULT_SOFWARE_WATCHDOG_SOURCE_CLOCK_FREQ
# define MEMFAULT_SOFWARE_WATCHDOG_SOURCE_CLOCK_FREQ 0
#endif

#if MEMFAULT_SOFWARE_WATCHDOG_SOURCE < 0 || MEMFAULT_SOFWARE_WATCHDOG_SOURCE > 4
# error "MEMFAULT_SOFWARE_WATCHDOG_SOURCE must be between 0 and 3"
#endif

#ifndef MEMFAULT_EXC_HANDLER_WATCHDOG
# if MEMFAULT_SOFWARE_WATCHDOG_SOURCE == 0
#  error "Port expects following define to be set: -DMEMFAULT_EXC_HANDLER_WATCHDOG=LPIT0_Ch0_IRQHandler"
# else
#  error "Port expects following define to be set: -DMEMFAULT_EXC_HANDLER_WATCHDOG=LPIT0_Ch<N>_IRQHandler"
# endif
#endif

static void prv_configure_irq(IRQn_Type irqn) {
  const uint32_t iser_reg_idx = irqn >> 5;
  const uint32_t iser_bit_idx = irqn % 32;
  S32_NVIC->ISER[iser_reg_idx] = (1UL << iser_bit_idx);

  // highest priority (0) so we can catch hangs in lower priority isrs
  S32_NVIC->IP[irqn] = 0;
}

static int prv_lpit_with_timeout(uint32_t timeout_ms) {
  const uint32_t pcc = PCC->PCCn[PCC_LPIT_INDEX];
  const uint32_t pcs = (pcc & PCC_PCCn_PCS_MASK) >> PCC_PCCn_PCS_SHIFT;

  // refer to "Table 27-9. Peripheral module clocking" of S32K-RM for valid options
  const char *clock_name;
  uint32_t clock_div2 = 0;
  uint32_t src_clock_freq = 0;

  switch (pcs) {
    case 1:
      clock_name = "SOSCDIV2";
      clock_div2 = (SCG->SOSCDIV & SCG_SOSCDIV_SOSCDIV2_MASK) >> SCG_SOSCDIV_SOSCDIV2_SHIFT;
      src_clock_freq = MEMFAULT_SOFWARE_WATCHDOG_SOURCE_CLOCK_FREQ;
      break;
    case 2:
      clock_name = "SIRCDIV2";
      clock_div2 = (SCG->SIRCDIV & SCG_SIRCDIV_SIRCDIV2_MASK) >> SCG_SIRCDIV_SIRCDIV2_SHIFT;
      src_clock_freq = ((SCG->SIRCCFG & SCG_SIRCCFG_RANGE_MASK) != 0) ?
          FEATURE_SCG_SIRC_HIGH_RANGE_FREQ : 2000000U;
      break;
    case 3:
      clock_name = "FIRCDIV2";
      clock_div2 = (SCG->FIRCDIV & SCG_FIRCDIV_FIRCDIV2_MASK) >> SCG_FIRCDIV_FIRCDIV2_SHIFT;
      src_clock_freq = FEATURE_SCG_FIRC_FREQ0;
      break;
    case 6:
      clock_name = "SPLLDIV2";
      clock_div2 = (SCG->SPLLDIV & SCG_SPLLDIV_SPLLDIV2_MASK) >> SCG_SPLLDIV_SPLLDIV2_SHIFT;
      src_clock_freq = MEMFAULT_SOFWARE_WATCHDOG_SOURCE_CLOCK_FREQ;
    default:
      MEMFAULT_LOG_ERROR("Illegal clock source for LPIT. PCC=0x%" PRIx32" PCS=0x%" PRIx32, pcc, pcs);
      return -1;
  }

  if (clock_div2 == 0) {
    MEMFAULT_LOG_ERROR("LPIT source clock (%s) not enabled", clock_name);
    return -2;
  }

  if (src_clock_freq == 0) {
    MEMFAULT_LOG_ERROR("-DMEMFAULT_SOFWARE_WATCHDOG_SOURCE_CLOCK_FREQ=<freq_hz> required");
    return -3;
  }

  const uint32_t clock_freq_hz = src_clock_freq >> (clock_div2 - 1);

  MEMFAULT_LOG_DEBUG("Configuring SW Watchdog. Source=%s, Timeout=%dms, Src Clock=%dHz",
                     clock_name, (int)timeout_ms, (int)clock_freq_hz);

  const uint64_t desired_tval = ((uint64_t)timeout_ms * clock_freq_hz) / 1000;
  if (desired_tval > UINT32_MAX) {
    const uint32_t max_seconds = ((uint64_t)UINT32_MAX * 1000) / clock_freq_hz;
    MEMFAULT_LOG_ERROR("Can't configure software watchdog of %d sec. Max=%"PRIu32" sec",
                       MEMFAULT_WATCHDOG_SW_TIMEOUT_SECS, max_seconds);
    return -4;
  }

  if ((LPIT0->MCR & LPIT_MCR_M_CEN_MASK) == 0) {
    LPIT0->MCR |= LPIT_MCR_M_CEN(1);
    // per "48.5.2 Initialization" of S32K-RM, must wait 4 peripheral clock
    // cycles "to allow time for clock synchronization and reset de-assertion"

    // NB: this will wind up being more cycles than needed since each loop iteration is
    // more than one instruction but will still be very fast and satisfy the initialization req
    const uint32_t core_to_periph_max_ratio = 256;
    for (volatile uint32_t delay = 0; delay < core_to_periph_max_ratio * 4; delay++) { }
  }

  // disable the timer, we are about to configure it!
  LPIT0->TMR[MEMFAULT_SOFWARE_WATCHDOG_SOURCE].TCTRL &= ~LPIT_TMR_TCTRL_T_EN(1);

  // Set up the countdown to match the desired watchdog timeout
  LPIT0->TMR[MEMFAULT_SOFWARE_WATCHDOG_SOURCE].TVAL = (uint32_t)desired_tval;

  // Steps
  //  1. Clear any pending ISRs
  //  2. Enable Timer countdown interrupt
  //  3. Enable ISR in NVIC
  //  4. Use highest priority so we can catch hangs in ISRs
  //     of lower priorities
  switch (MEMFAULT_SOFWARE_WATCHDOG_SOURCE) {
    case 0:
      LPIT0->MSR |= LPIT_MSR_TIF0(0);
      LPIT0->MIER |= LPIT_MIER_TIE0(1);
      prv_configure_irq(LPIT0_Ch0_IRQn);
      break;
    case 1:
      LPIT0->MSR |= LPIT_MSR_TIF1(0);
      LPIT0->MIER |= LPIT_MIER_TIE1(1);
      prv_configure_irq(LPIT0_Ch1_IRQn);
      break;
    case 2:
      LPIT0->MSR |= LPIT_MSR_TIF2(0);
      LPIT0->MIER |= LPIT_MIER_TIE2(1);
      prv_configure_irq(LPIT0_Ch2_IRQn);
      break;
    case 3:
      LPIT0->MSR |= LPIT_MSR_TIF3(0);
      LPIT0->MIER |= LPIT_MIER_TIE3(1);
      prv_configure_irq(LPIT0_Ch3_IRQn);
      break;
    default:
      break;
  }

  LPIT0->TMR[MEMFAULT_SOFWARE_WATCHDOG_SOURCE].TCTRL =
      LPIT_TMR_TCTRL_MODE(0) | // 32 bit count down mode
      LPIT_TMR_TCTRL_T_EN(1);

  return 0;
}

int memfault_software_watchdog_enable(void) {
  const uint32_t timeout_ms = \
      MEMFAULT_WATCHDOG_SW_TIMEOUT_SECS * 1000;
  return prv_lpit_with_timeout(timeout_ms);
}

int memfault_software_watchdog_disable(void) {
  LPIT0->TMR[MEMFAULT_SOFWARE_WATCHDOG_SOURCE].TCTRL &= ~LPIT_TMR_TCTRL_T_EN(1);
  return 0;
}

int memfault_software_watchdog_feed(void) {
  // PER S32K-RM, "To abort the current timer cycle and start a timer period with a new value, the
  // timer channel must be disabled and enabled again"

  LPIT0->TMR[MEMFAULT_SOFWARE_WATCHDOG_SOURCE].TCTRL &= ~LPIT_TMR_TCTRL_T_EN(1);
  LPIT0->TMR[MEMFAULT_SOFWARE_WATCHDOG_SOURCE].TCTRL |= LPIT_TMR_TCTRL_T_EN(1);

  return 0;
}

int memfault_software_watchdog_update_timeout(uint32_t timeout_ms) {
  return prv_lpit_with_timeout(timeout_ms);
}
