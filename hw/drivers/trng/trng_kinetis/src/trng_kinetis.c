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
#include "os/mynewt.h"

#if MYNEWT_VAL(KINETIS_TRNG_USE_RNGA)
#define USE_RNGA 1
#include "fsl_rnga.h"
#elif MYNEWT_VAL(KINETIS_TRNG_USE_TRNG)
#define USE_TRNG 1
#include "fsl_trng.h"
#define TRNG_START(base)                                              \
    do {                                                              \
        (base)->MCTL &= ~TRNG_MCTL_PRGM_MASK;                         \
        (base)->MCTL |= TRNG_MCTL_ERR_MASK;                           \
    } while (0)
#define TRNG_STOP(base)                                               \
    do {                                                              \
        (base)->MCTL |= (TRNG_MCTL_PRGM_MASK | TRNG_MCTL_ERR_MASK);   \
    } while (0)
#else
#error "Unsupported TRNG interface"
#endif

#include "trng/trng.h"
#include "trng_kinetis/trng_kinetis.h"

static uint8_t rng_cache[ MYNEWT_VAL(KINETIS_TRNG_CACHE_LEN) ];
static uint16_t rng_cache_out;
static uint16_t rng_cache_in;
static struct os_mutex rng_cache_mu;
static os_stack_t *pstack;
static bool running;
static struct os_eventq rng_evtq;

#define TRNG_POLLER_PRIO (8)
#define TRNG_POLLER_STACK_SIZE OS_STACK_ALIGN(64)
static struct os_task poller_task;

static inline void
kinetis_trng_start(void)
{
    struct os_event evt;

#if USE_RNGA
    RNGA_SetMode(RNG, kRNGA_ModeNormal);
#elif USE_TRNG
    TRNG_START(TRNG0);
#endif
    running = true;

    evt.ev_queued = 0;
    evt.ev_arg = NULL;
    (void)os_eventq_put(&rng_evtq, &evt);
}

static inline void
kinetis_trng_stop(void)
{
#if USE_RNGA
    RNGA_SetMode(RNG, kRNGA_ModeSleep);
#elif USE_TRNG
    TRNG_STOP(TRNG0);
#endif

   running = false;
}

static size_t
kinetis_trng_read(struct trng_dev *trng, void *ptr, size_t size)
{
    size_t num_read;

    os_mutex_pend(&rng_cache_mu, OS_TIMEOUT_NEVER);

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

    rng_cache_out = (rng_cache_out + num_read) % sizeof(rng_cache);

    if (num_read > 0) {
        kinetis_trng_start();
    }

    os_mutex_release(&rng_cache_mu);

    return num_read;
}

static uint32_t
kinetis_trng_get_u32(struct trng_dev *trng)
{
    union {
        uint32_t v32;
        uint8_t v8[4];
    } val;
    size_t num;

    num = kinetis_trng_read(trng, &val.v8, sizeof(val.v8));
    while (num < sizeof(val.v8)) {
        os_sched(NULL);
        num += kinetis_trng_read(trng, &val.v8[num], sizeof(val.v8) - num);
    }

    return val.v32;
}

static void
trng_poller_handler(void *arg)
{
    int8_t i;
    uint8_t data[4];
    int rc;

    while (1) {
        if (running) {
#if USE_RNGA
            rc = RNGA_GetRandomData(RNG, data, sizeof(uint32_t));
#else
            rc = TRNG_GetRandomData(TRNG0, data, sizeof(uint32_t));
#endif
            if (rc == 0) {
                os_mutex_pend(&rng_cache_mu, OS_TIMEOUT_NEVER);
                for (i = 0; i < 4; i++) {
                    rng_cache[rng_cache_in++] = data[i];

                    if (rng_cache_in >= sizeof(rng_cache)) {
                        rng_cache_in = 0;
                    }

                    if ((rng_cache_in + 1) % sizeof(rng_cache) == rng_cache_out) {
                        kinetis_trng_stop();
                        break;
                    }
                }
                os_mutex_release(&rng_cache_mu);
            }
        } else {
            (void)os_eventq_get(&rng_evtq);
        }
    }
}

static int
kinetis_trng_dev_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    struct trng_dev *trng;
#if USE_TRNG
    trng_config_t default_config;
#endif

    trng = (struct trng_dev *)dev;
    assert(trng);

    if (!(dev->od_flags & OS_DEV_F_STATUS_OPEN)) {
        rng_cache_out = 0;
        rng_cache_in = 0;

#if USE_RNGA
        RNGA_Init(RNG);
        RNGA_Seed(RNG, SIM->UIDL);
#elif USE_TRNG
        (void)TRNG_GetDefaultConfig(&default_config);
        TRNG_Init(TRNG0, &default_config);
#endif

        kinetis_trng_start();
    }

    return 0;
}

int
kinetis_trng_dev_init(struct os_dev *dev, void *arg)
{
    struct trng_dev *trng;
    int rc;

    trng = (struct trng_dev *)dev;
    assert(trng);

    OS_DEV_SETHANDLERS(dev, kinetis_trng_dev_open, NULL);

    trng->interface.get_u32 = kinetis_trng_get_u32;
    trng->interface.read = kinetis_trng_read;

    os_eventq_init(&rng_evtq);
    os_mutex_init(&rng_cache_mu);

    pstack = malloc(sizeof(os_stack_t) * TRNG_POLLER_STACK_SIZE);
    assert(pstack);

    rc = os_task_init(&poller_task, "trng_poller", trng_poller_handler, NULL,
            TRNG_POLLER_PRIO, OS_WAIT_FOREVER, pstack, TRNG_POLLER_STACK_SIZE);
    if (rc) {
        return rc;
    }

    return 0;
}
