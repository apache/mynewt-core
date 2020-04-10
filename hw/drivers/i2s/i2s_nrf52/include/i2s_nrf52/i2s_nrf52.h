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

#ifndef _I2S_NRF52_H
#define _I2S_NRF52_H

#include <stdint.h>
#include <nrfx/nrfx.h>
#include <nrfx/drivers/include/nrfx_i2s.h>

struct i2s;

struct i2s_cfg {
    nrfx_i2s_config_t nrfx_i2s_cfg;

    uint32_t sample_rate;

    struct i2s_buffer_pool *pool;
};

#endif /* _I2S_NRF52_H */
