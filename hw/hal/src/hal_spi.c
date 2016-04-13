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
#include <hal/hal_spi.h>
#include <hal/hal_spi_int.h>

struct hal_spi *
hal_spi_init(enum system_device_id pin) 
{
    return bsp_get_hal_spi(pin);
}

int 
hal_spi_config(struct hal_spi *pspi, struct hal_spi_settings *psettings) 
{
    if (pspi && pspi->driver_api && pspi->driver_api->hspi_config) {
        return pspi->driver_api->hspi_config(pspi, psettings);
    }
    return -1;
}

int 
hal_spi_master_transfer(struct hal_spi *pspi, uint16_t val) 
{
    if (pspi && pspi->driver_api && pspi->driver_api->hspi_master_transfer) {
        return pspi->driver_api->hspi_master_transfer(pspi, val);
    }
    return -1;
}
