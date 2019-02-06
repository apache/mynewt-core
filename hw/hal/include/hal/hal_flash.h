/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/**
 * @addtogroup HAL
 * @{
 *   @defgroup HALFlash HAL Flash
 *   @{
 */

#ifndef H_HAL_FLASH_
#define H_HAL_FLASH_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

int hal_flash_ioctl(uint8_t flash_id, uint32_t cmd, void *args);

/**
 * @brief Reads a block of data from flash.
 *
 * @param flash_id              The ID of the flash device to read from.
 * @param address               The address to read from.
 * @param dst                   A buffer to fill with data read from flash.
 * @param num_bytes             The number of bytes to read.
 *
 * @return                      0 on success;
 *                              SYS_EINVAL on bad argument error;
 *                              SYS_EIO on flash driver error.
 */
int hal_flash_read(uint8_t flash_id, uint32_t address, void *dst,
  uint32_t num_bytes);

/**
 * @brief Writes a block of data to flash.
 *
 * @param flash_id              The ID of the flash device to write to.
 * @param address               The address to write to.
 * @param src                   A buffer containing the data to be written.
 * @param num_bytes             The number of bytes to write.
 *
 * @return                      0 on success;
 *                              SYS_EINVAL on bad argument error;
 *                              SYS_EACCES if flash region is write protected;
 *                              SYS_EIO on flash driver error.
 */
int hal_flash_write(uint8_t flash_id, uint32_t address, const void *src,
  uint32_t num_bytes);

/**
 * @brief Erases a single flash sector.
 *
 * @param flash_id              The ID of the flash device to erase.
 * @param sector_address        An address within the sector to erase.
 *
 * @return                      0 on success;
 *                              SYS_EINVAL on bad argument error;
 *                              SYS_EACCES if flash region is write protected;
 *                              SYS_EIO on flash driver error.
 */
int hal_flash_erase_sector(uint8_t flash_id, uint32_t sector_address);

/**
 * @brief Erases a contiguous sequence of flash sectors.
 *
 * If the specified range does not correspond to a whole number of sectors,
 * any partially-specified sectors are fully erased.  For example, if a device
 * has 1024-byte sectors, then these arguments:
 *     o address: 300
 *     o num_bytes: 1000
 * cause the first two sectors to be erased in their entirety.
 *
 * @param flash_id              The ID of the flash device to erase.
 * @param address               An address within the sector to begin the erase
 *                                  at.
 * @param num_bytes             The length, in bytes, of the region to erase.
 *
 * @return                      0 on success;
 *                              SYS_EINVAL on bad argument error;
 *                              SYS_EACCES if flash region is write protected;
 *                              SYS_EIO on flash driver error.
 */
int hal_flash_erase(uint8_t flash_id, uint32_t address, uint32_t num_bytes);

/**
 * @brief Determines if the specified region of flash is completely unwritten.
 *
 * @param id                    The ID of the flash hardware to inspect.
 * @param address               The starting address of the check.
 * @param dst                   A buffer to hold the contents of the flash
 *                                  region.  This must be at least `num_bytes`
 *                                  large.
 * @param num_bytes             The number of bytes to check.
 *
 * @return                      0 if any written bytes were detected;
 *                              1 if the region is completely unwritten;
 *                              SYS_EINVAL on bad argument error;
 *                              SYS_EIO on flash driver error.
 */
int hal_flash_isempty(uint8_t flash_id, uint32_t address, void *dst,
                      uint32_t num_bytes);

/**
 * @brief Determines if the specified region of flash is completely unwritten.
 *
 * This function is like `hal_flash_isempty()`, except the caller does not need
 * to provide a buffer.  Instead, a buffer of size
 * MYNEWT_VAL(HAL_FLASH_VERIFY_BUF_SZ) is allocated on the stack.
 *
 * @param id                    The ID of the flash hardware to inspect.
 * @param address               The starting address of the check.
 * @param num_bytes             The number of bytes of flash to check.
 *
 * @return                      0 if any written bytes were detected;
 *                              1 if the region is completely unwritten;
 *                              SYS_EINVAL on bad argument error;
 *                              SYS_EIO on flash driver error.
 */
int hal_flash_isempty_no_buf(uint8_t id, uint32_t address, uint32_t num_bytes);

/**
 * @brief Determines the minimum write alignment of a flash device.
 *
 * @param id                    The ID of the flash hardware to check.
 *
 * @return                      The flash device's minimum write alignment.
 */
uint8_t hal_flash_align(uint8_t flash_id);

/**
 * @brief Determines the value of an erased byte for a particular flash device.
 *
 * @param id                    The ID of the flash hardware to check.
 *
 * @return                      The value of an erased byte.
 */
uint8_t hal_flash_erased_val(uint8_t flash_id);

/**
 * @brief Initializes all flash devices in the system.
 *
 * @return                      0 on success;
 *                              SYS_EIO on flash driver error.
 */
int hal_flash_init(void);

/**
 * @brief Set or clears write protection
 *
 * This function allows to disable write to the device if for some reason
 * (i.e. low power state) writes could result in data corruption.
 *
 * @param id          The ID of the flash
 * @param protect     1 - disable writes
 *                    0 - enable writes
 *
 * @return           SYS_EINVAL - if flash id is not valid
 *                   SYS_OK - on success
 */
int hal_flash_write_protect(uint8_t id, uint8_t protect);

#ifdef __cplusplus
}
#endif

#endif /* H_HAL_FLASH_ */

/**
 *   @} HALFlash
 * @} HAL
 */
