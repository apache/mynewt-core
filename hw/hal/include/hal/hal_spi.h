/**
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


#ifndef H_HAL_SPI_
#define H_HAL_SPI_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <bsp/bsp_sysid.h>

enum hal_spi_data_mode{
    HAL_SPI_MODE0,
    HAL_SPI_MODE1,
    HAL_SPI_MODE2,
    HAL_SPI_MODE3,
} ;

enum hal_spi_data_order{
    HAL_SPI_MSB_FIRST,
    HAL_SPI_LSB_FIRST,
} ;

enum hal_spi_word_size{
    HAL_SPI_WORD_SIZE_8BIT,
    HAL_SPI_WORD_SIZE_9BIT,
};

typedef uint32_t hal_spi_baudrate;

/**
 * since one spi device can control multiple devices, some configuration
 * can be changed on the fly from the hal
 */
struct hal_spi_settings {
    enum hal_spi_data_mode  data_mode;
    enum hal_spi_data_order data_order;
    enum hal_spi_word_size  word_size;
    hal_spi_baudrate        baudrate;
};

/**
 * Initialize the SPI, given by spi_num.
 *
 * @param spi_num The number of the SPI to initialize
 * @param cfg HW/MCU specific configuration,
 *            passed to the underlying
 *            implementation, providing extra configuration.
 *
 * @return 0 on success, non-zero error code on failure.
 */
int hal_spi_init(uint8_t spi_num, void *cfg);

/**
 * Configure the spi.
 *
 * @param spi_num The number of the SPI to configure.
 * @param psettings The settings to configure this SPI with
 *
 * @return 0 on success, non-zero error code on failure.
 */
int hal_spi_config(uint8_t spi_num, struct hal_spi_settings *psettings);

/**
 * Do a blocking master spi transfer of one SPI data word.
 * The data to send is an 8 or 9-bit pattern (depending on configuration)
 * stored in <tx>.  NOTE: This does not send multiple bytes. The argument is
 * a 16-bit number to allow up to 9-bit SPI data words.
 *
 * @param spi_num The number of the SPI to TX data on
 * @param tx The data to TX
 *
 * @return The data received from the remote device, or negative on error.
 */
int hal_spi_master_transfer(uint8_t spi_num, uint16_t tx);


#ifdef __cplusplus
}
#endif

#endif /* H_HAL_SPI_ */
