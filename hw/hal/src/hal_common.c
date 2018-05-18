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

const char *
hal_reset_cause_str(void)
{
    enum hal_reset_reason cause;

    cause = hal_reset_cause();
    switch (cause) {
    case HAL_RESET_POR:
        return "Power on Reset";
    case HAL_RESET_PIN:
        return "Reset Pin";
    case HAL_RESET_WATCHDOG:
        return "Watchdog";
    case HAL_RESET_SOFT:
        return "Soft Reset";
    case HAL_RESET_BROWNOUT:
        return "Low Voltage";
    case HAL_RESET_REQUESTED:
        return "User Requested";
    default:
        return "Unknown";
    }
}
