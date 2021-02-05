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

#ifndef __KINETIS_HAL_H_
#define __KINETIS_HAL_H_

#ifdef __cplusplus
extern "C" {
#endif

struct nxp_hal_i2c_cfg {
    int8_t pin_scl;
    int8_t pin_sda;
    uint32_t frequency;
};

struct nxp_hal_spi_cfg {
    uint32_t clk_pin;
    uint32_t pcs_pin;
    uint32_t sout_pin;
    uint32_t sin_pin;
};

#ifdef __cplusplus
}
#endif

#endif /* __KINETIS_HAL_H_ */
