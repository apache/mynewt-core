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

#include <string.h>
#include "mcu/cmsis_nvic.h"
#include "mcu/stm32_hal.h"
#include "trng/trng.h"
#include "trng_stm32/trng_stm32.h"

static uint8_t rng_cache[ MYNEWT_VAL(STM32_TRNG_CACHE_LEN) ];
static uint16_t rng_cache_out;
static uint16_t rng_cache_in;

static RNG_HandleTypeDef g_hrng;

static void
stm32_rng_start(void)
{
    __HAL_RNG_ENABLE(&g_hrng);
    __HAL_RNG_ENABLE_IT(&g_hrng);
}

static void
stm32_rng_stop(void)
{
    __HAL_RNG_DISABLE(&g_hrng);
}

static void
stm32_rng_irq_handler(void)
{
    int8_t i;

    HAL_RNG_IRQHandler(&g_hrng);
    if (g_hrng.State == HAL_RNG_STATE_READY) {
        for (i = 24; i >= 0; i -= 8) {
            rng_cache[rng_cache_in++] = (g_hrng.RandomNumber >> i) & 0xff;

            if (rng_cache_in >= sizeof(rng_cache)) {
                rng_cache_in = 0;
            }

            if ((rng_cache_in + 1) % sizeof(rng_cache) == rng_cache_out) {
                stm32_rng_stop();
                return;
            }
        }
    }
    __HAL_RNG_ENABLE_IT(&g_hrng);
}

static size_t
stm32_trng_read(struct trng_dev *trng, void *ptr, size_t size)
{
    os_sr_t sr;
    size_t num_read;

    OS_ENTER_CRITICAL(sr);

    if (rng_cache_out <= rng_cache_in) {
        size = min(size, rng_cache_in - rng_cache_out);
        memcpy(ptr, &rng_cache[rng_cache_out], size);
        num_read = size;
    } else if (rng_cache_out + size <= sizeof(rng_cache)) {
        memcpy(ptr, &rng_cache[rng_cache_out], size);
        num_read = size;
    } else {
        num_read = sizeof(rng_cache) - rng_cache_out;
        memcpy(ptr, &rng_cache[rng_cache_out], num_read);

        size -= num_read;
        ptr += num_read;

        size = min(size, rng_cache_in);
        memcpy(ptr, rng_cache, size);
        num_read += size;
    }

    rng_cache_out += num_read;
    rng_cache_out %= sizeof(rng_cache);

    if (num_read > 0) {
        stm32_rng_start();
    }

    OS_EXIT_CRITICAL(sr);

    return num_read;
}

static uint32_t
stm32_trng_get_u32(struct trng_dev *trng)
{
    union {
        uint32_t v32;
        uint8_t v8[4];
    } val;
    size_t num;

    num = stm32_trng_read(trng, &val.v8, sizeof(val.v8));
    while (num < sizeof(val.v8)) {
        os_sched(NULL);
        num += stm32_trng_read(trng, &val.v8[num], sizeof(val.v8) - num);
    }

    return val.v32;
}

static int
stm32_trng_dev_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    struct trng_dev *trng;

    trng = (struct trng_dev *)dev;
    assert(trng);

    if (!(dev->od_flags & OS_DEV_F_STATUS_OPEN)) {
        rng_cache_out = 0;
        rng_cache_in = 0;

        __HAL_RCC_RNG_CLK_ENABLE();

        g_hrng.Instance = RNG;
        g_hrng.State = HAL_RNG_STATE_BUSY;
        HAL_RNG_Init(&g_hrng);

        NVIC_SetPriority(RNG_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
        NVIC_SetVector(RNG_IRQn, (uint32_t)stm32_rng_irq_handler);
        NVIC_EnableIRQ(RNG_IRQn);

        stm32_rng_start();
    }

    return 0;
}

int
stm32_trng_dev_init(struct os_dev *dev, void *arg)
{
    struct trng_dev *trng;

    trng = (struct trng_dev *)dev;
    assert(trng);

    OS_DEV_SETHANDLERS(dev, stm32_trng_dev_open, NULL);

    trng->interface.get_u32 = stm32_trng_get_u32;
    trng->interface.read = stm32_trng_read;

    return 0;
}
