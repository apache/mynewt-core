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

#include "hal/hal_spi.h"
#include "hal/hal_system.h"

void _exit(int status);

void
_exit(int status)
{
    hal_system_reset();
}

/**
 * Extracts CPOL and CPHA values from a data-mode constant.
 *
 * @param data_mode             The HAL_SPI_MODE value to convert.
 * @param out_cpol              The CPOL gets written here on success.
 * @param out_cpha              The CPHA gets written here on success.
 *
 * @return                      0 on success; nonzero on invalid input.
 */
int
hal_spi_data_mode_breakout(uint8_t data_mode, int *out_cpol, int *out_cpha)
{
    switch (data_mode) {
    case HAL_SPI_MODE0:
        *out_cpol = 0;
        *out_cpha = 0;
        return 0;

    case HAL_SPI_MODE1:
        *out_cpol = 0;
        *out_cpha = 1;
        return 0;

    case HAL_SPI_MODE2:
        *out_cpol = 1;
        *out_cpha = 0;
        return 0;

    case HAL_SPI_MODE3:
        *out_cpol = 1;
        *out_cpha = 1;
        return 0;

    default:
        return -1;
    }
}
