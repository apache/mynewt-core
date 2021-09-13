//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A Software Watchdog implementation backed by the
//! STM32H7xx LPTIM peripheral & the STM32CubeH7 HAL API.
//!
//! The STM32H7 HAL can be cloned from https://github.com/STMicroelectronics/STM32CubeH7
//! or downloaded as a zip from https://www.st.com/en/embedded-software/stm32cubeh7.html
//!
//! The LPTIM is configured to use the LSI clock source so that the counter continues to run
//! while the system is in low power modes (just like the hardware backed IWDG). By setting
//! MEMFAULT_WATCHDOG_SW_TIMEOUT_SECS to a timeout less than the hardware watchdog, we can
//! guarantee a capture of a coredump when the system is in a wedged state.

#include "memfault/ports/watchdog.h"

#include <stdbool.h>

#include "stm32h7xx_hal.h"

#include "memfault/config.h"
#include "memfault/core/compiler.h"
#include "memfault/core/math.h"

#ifndef MEMFAULT_SOFWARE_WATCHDOG_SOURCE
# define MEMFAULT_SOFWARE_WATCHDOG_SOURCE LPTIM2_BASE
#endif

#ifndef MEMFAULT_LPTIM_ENABLE_FREEZE_DBGMCU
// By default, we automatically halt the software watchdog timer when
// the core is halted in debug mode. This prevents a watchdog from firing
// immediately upon resumption.
# define MEMFAULT_LPTIM_ENABLE_FREEZE_DBGMCU 1
#endif

#ifndef MEMFAULT_EXC_HANDLER_WATCHDOG
# if MEMFAULT_SOFWARE_WATCHDOG_SOURCE == LPTIM2_BASE
#  error "Port expects following define to be set: -DMEMFAULT_EXC_HANDLER_WATCHDOG=LPTIM2_IRQHandler"
# else
#  error "Port expects following define to be set: -DMEMFAULT_EXC_HANDLER_WATCHDOG=LPTIM<N>_IRQHandler"
# endif
#endif

#if ((MEMFAULT_SOFWARE_WATCHDOG_SOURCE != LPTIM1_BASE) && \
     (MEMFAULT_SOFWARE_WATCHDOG_SOURCE != LPTIM2_BASE) && \
     (MEMFAULT_SOFWARE_WATCHDOG_SOURCE != LPTIM3_BASE) && \
     (MEMFAULT_SOFWARE_WATCHDOG_SOURCE != LPTIM4_BASE) && \
     (MEMFAULT_SOFWARE_WATCHDOG_SOURCE != LPTIM5_BASE))
# error "MEMFAULT_SOFWARE_WATCHDOG_SOURCE must be one of LPTIM[1-5]_BASE"
#endif

//! We wire the LPTIM -> LSI so the clock frequency will be 32kHz
#define MEMFAULT_LPTIM_CLOCK_FREQ_HZ 32000U
#define MEMFAULT_LPTIM_PRESCALER 128
#define MEMFAULT_LPTIM_MAX_COUNT 0xFFFF

#define MEMFAULT_LPTIM_HZ (MEMFAULT_LPTIM_CLOCK_FREQ_HZ / MEMFAULT_LPTIM_PRESCALER)

#define MEMFAULT_MS_PER_SEC 1000
#define MEMFAULT_LPTIM_MAX_TIMEOUT_SEC ((MEMFAULT_LPTIM_MAX_COUNT + 1) / MEMFAULT_LPTIM_HZ)

#if MEMFAULT_WATCHDOG_SW_TIMEOUT_SECS > MEMFAULT_LPTIM_MAX_TIMEOUT_SEC
#error "MEMFAULT_WATCHDOG_SW_TIMEOUT_SECS exceeds maximum value supported by hardware"
#endif

static LPTIM_HandleTypeDef s_lptim_cfg;

static void prv_lptim_clock_config(RCC_PeriphCLKInitTypeDef *config) {
  switch ((uint32_t)MEMFAULT_SOFWARE_WATCHDOG_SOURCE) {
    case LPTIM1_BASE:
      *config = (RCC_PeriphCLKInitTypeDef )  {
        .PeriphClockSelection = RCC_PERIPHCLK_LPTIM1,
        .Lptim1ClockSelection = RCC_LPTIM1CLKSOURCE_LSI
      };
      break;

    case LPTIM2_BASE:
      *config = (RCC_PeriphCLKInitTypeDef )  {
        .PeriphClockSelection = RCC_PERIPHCLK_LPTIM2,
        .Lptim2ClockSelection = RCC_LPTIM2CLKSOURCE_LSI
      };
      break;

    case LPTIM3_BASE:
      *config = (RCC_PeriphCLKInitTypeDef )  {
        .PeriphClockSelection = RCC_PERIPHCLK_LPTIM3,
        .Lptim345ClockSelection = RCC_LPTIM3CLKSOURCE_LSI
      };
      break;

    case LPTIM4_BASE:
      *config = (RCC_PeriphCLKInitTypeDef )  {
        .PeriphClockSelection = RCC_PERIPHCLK_LPTIM4,
        .Lptim345ClockSelection = RCC_LPTIM4CLKSOURCE_LSI
      };
      break;

    case LPTIM5_BASE:
      *config = (RCC_PeriphCLKInitTypeDef )  {
        .PeriphClockSelection = RCC_PERIPHCLK_LPTIM5,
        .Lptim345ClockSelection = RCC_LPTIM4CLKSOURCE_LSI
      };
      break;

    default:
      *config =  (RCC_PeriphCLKInitTypeDef) { 0 };
      break;
  }
}

static void prv_lptim_clk_enable(void) {
  switch (MEMFAULT_SOFWARE_WATCHDOG_SOURCE) {
    case LPTIM1_BASE:
      __HAL_RCC_LPTIM1_CLK_ENABLE();
      __HAL_RCC_LPTIM1_FORCE_RESET();
      __HAL_RCC_LPTIM1_RELEASE_RESET();
      break;

    case LPTIM2_BASE:
      __HAL_RCC_LPTIM2_CLK_ENABLE();
      __HAL_RCC_LPTIM2_FORCE_RESET();
      __HAL_RCC_LPTIM2_RELEASE_RESET();
      break;

    case LPTIM3_BASE:
      __HAL_RCC_LPTIM3_CLK_ENABLE();
      __HAL_RCC_LPTIM3_FORCE_RESET();
      __HAL_RCC_LPTIM3_RELEASE_RESET();
      break;

    case LPTIM4_BASE:
      __HAL_RCC_LPTIM4_CLK_ENABLE();
      __HAL_RCC_LPTIM4_FORCE_RESET();
      __HAL_RCC_LPTIM4_RELEASE_RESET();
      break;

    case LPTIM5_BASE:
      __HAL_RCC_LPTIM5_CLK_ENABLE();
      __HAL_RCC_LPTIM5_FORCE_RESET();
      __HAL_RCC_LPTIM5_RELEASE_RESET();
      break;

    default:
      break;
  }
}

static void prv_lptim_clk_freeze_during_dbg(void) {
#if MEMFAULT_LPTIM_ENABLE_FREEZE_DBGMCU
  switch (MEMFAULT_SOFWARE_WATCHDOG_SOURCE) {
    case LPTIM1_BASE:
      __HAL_DBGMCU_FREEZE_LPTIM1();
      break;

    case LPTIM2_BASE:
      __HAL_DBGMCU_FREEZE_LPTIM2();
      break;

    case LPTIM3_BASE:
      __HAL_DBGMCU_FREEZE_LPTIM3();
      break;

    case LPTIM4_BASE:
      __HAL_DBGMCU_FREEZE_LPTIM4();
      break;

    case LPTIM5_BASE:
      __HAL_DBGMCU_FREEZE_LPTIM5();
      break;

    default:
      break;
  }
#endif
}

static void prv_lptim_irq_enable(void) {
  switch (MEMFAULT_SOFWARE_WATCHDOG_SOURCE) {
    case LPTIM1_BASE:
       NVIC_ClearPendingIRQ(LPTIM1_IRQn);
       NVIC_EnableIRQ(LPTIM1_IRQn);
      break;

    case LPTIM2_BASE:
       NVIC_ClearPendingIRQ(LPTIM2_IRQn);
       NVIC_EnableIRQ(LPTIM2_IRQn);
      break;

    case LPTIM3_BASE:
       NVIC_ClearPendingIRQ(LPTIM3_IRQn);
       NVIC_EnableIRQ(LPTIM3_IRQn);
      break;

    case LPTIM4_BASE:
       NVIC_ClearPendingIRQ(LPTIM4_IRQn);
       NVIC_EnableIRQ(LPTIM4_IRQn);
      break;

    case LPTIM5_BASE:
       NVIC_ClearPendingIRQ(LPTIM5_IRQn);
       NVIC_EnableIRQ(LPTIM5_IRQn);
      break;

    default:
      break;
  }
}

int memfault_software_watchdog_enable(void) {
  // We will drive the Low Power Timer (LPTIM) from the Low-speed internal (LSI) oscillator
  // (~32kHz). This source will run while in low power modes (just like the IWDG hardware watchdog)
  const bool lsi_on = (RCC->CSR & RCC_CSR_LSION) == RCC_CSR_LSION;
  if (!lsi_on) {
    __HAL_RCC_LSI_ENABLE();

    const uint32_t tickstart = HAL_GetTick();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == 0) {
      if ((HAL_GetTick() - tickstart) > LSI_TIMEOUT_VALUE) {
        return HAL_TIMEOUT;
      }
    }
  }

  // The LPTIM can be driven from multiple clock sources so we need to explicitly connect
  // it to the LSI clock that we just enabled.
  RCC_PeriphCLKInitTypeDef lptim_clock_config;
  prv_lptim_clock_config(&lptim_clock_config);
  HAL_StatusTypeDef rv = HAL_RCCEx_PeriphCLKConfig(&lptim_clock_config);
  if (rv != HAL_OK) {
    return rv;
  }

  // Enable the LPTIM clock and reset the peripheral
  prv_lptim_clk_enable();
  prv_lptim_clk_freeze_during_dbg();

  s_lptim_cfg = (LPTIM_HandleTypeDef) {
    .Instance = (LPTIM_TypeDef *)MEMFAULT_SOFWARE_WATCHDOG_SOURCE,
    .Init = {
      .Clock = {
        .Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC,
#if MEMFAULT_LPTIM_PRESCALER == 128
        .Prescaler = LPTIM_PRESCALER_DIV128,
#else
#error "Unexpected Prescaler value"
#endif
      },
      .Trigger = {
        .Source = LPTIM_TRIGSOURCE_SOFTWARE,
      },
      .OutputPolarity = LPTIM_OUTPUTPOLARITY_HIGH,
      .UpdateMode = LPTIM_UPDATE_IMMEDIATE,
      .CounterSource = LPTIM_COUNTERSOURCE_INTERNAL,

      // NB: not used in config but HAL expects valid values here
      .Input1Source = LPTIM_INPUT1SOURCE_GPIO,
      .Input2Source = LPTIM_INPUT2SOURCE_GPIO,
    },
    .State = HAL_LPTIM_STATE_RESET,
  };
  rv = HAL_LPTIM_Init(&s_lptim_cfg);
  if (rv != HAL_OK) {
    return rv;
  }

  prv_lptim_irq_enable();

  const uint32_t desired_timeout_ms =
      MEMFAULT_WATCHDOG_SW_TIMEOUT_SECS * MEMFAULT_MS_PER_SEC;
  memfault_software_watchdog_update_timeout(desired_timeout_ms);
  return 0;
}

int memfault_software_watchdog_feed(void) {
  if ((s_lptim_cfg.Instance->CR & LPTIM_CR_COUNTRST) != 0) {
    // A COUNTRST is already in progress, no work to do
    return 0;
  }
  __HAL_LPTIM_RESET_COUNTER(&s_lptim_cfg);

  return 0;
}

int memfault_software_watchdog_update_timeout(uint32_t timeout_ms) {
  if (timeout_ms > (MEMFAULT_LPTIM_MAX_TIMEOUT_SEC * MEMFAULT_MS_PER_SEC)) {
    return -1;
  }

  HAL_StatusTypeDef rv = HAL_LPTIM_Counter_Stop(&s_lptim_cfg);
  if (rv != HAL_OK) {
    return rv;
  }

  __HAL_LPTIM_CLEAR_FLAG(&s_lptim_cfg, LPTIM_IT_ARRM);
  __HAL_LPTIM_DISABLE_IT(&s_lptim_cfg, LPTIM_IT_ARRM);

  const uint32_t ticks = MEMFAULT_MIN((timeout_ms * MEMFAULT_LPTIM_HZ) / MEMFAULT_MS_PER_SEC,
                                      MEMFAULT_LPTIM_MAX_COUNT);

  rv = HAL_LPTIM_Counter_Start(&s_lptim_cfg, ticks);
  if (rv != HAL_OK) {
    return rv;
  }

  __HAL_LPTIM_ENABLE_IT(&s_lptim_cfg, LPTIM_IT_ARRM);

  return 0;
}

int memfault_software_watchdog_disable(void) {
  // Clear and disable interrupts
  __HAL_LPTIM_DISABLE_IT(&s_lptim_cfg, LPTIM_IT_ARRM);

  // Stop the counter
  return HAL_LPTIM_Counter_Stop(&s_lptim_cfg);
}
