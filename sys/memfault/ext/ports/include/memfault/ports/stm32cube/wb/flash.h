#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Utilities that can be used alongside STM32WB coredump storage flash port

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//! Check FLASH->ECCR for ECC error and clear error if it's within address range
//!
//! @note The STM32WB ECCs every double-word write. There is a low probability that
//! a write fails due to a power glitch / brown out or other phenomena. If the error is not
//! correctable (> 1 bit), an NMI will be generated and the NMI_Handler will be invoked.
//! From that handler, one can programmatically clear the error by writing zeros to
//! the failing address. See https://mflt.io/stm32-ecc-error-recovery for more context
//!
//! @param[in] start_addr The start address of the range to clear ECC errors in
//! @param[in] end_addr The end address of the range to clear ECC errors in
//! @param[out] corrupted_address The address that was corrupted or zero if no
//!  error was found
//!
//! @return true if no error was detected or error was successfully cleared,
//!  false otherwise
bool memfault_stm32cubewb_flash_clear_ecc_errors(
    uint32_t start_addr, uint32_t end_addr, uint32_t *corrupted_address);

#ifdef __cplusplus
}
#endif
