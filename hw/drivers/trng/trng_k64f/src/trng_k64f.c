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
#include "fsl_rnga.h"
#include "trng/trng.h"
#include "trng_k64f/trng_k64f.h"

static uint8_t rng_cache[ MYNEWT_VAL(K64F_TRNG_CACHE_LEN) ];
static uint16_t rng_cache_out;
static uint16_t rng_cache_in;
static struct os_mutex rng_cache_mu;
static os_stack_t *pstack;
static bool running;
static struct os_eventq rng_evtq;

#define RNGA_POLLER_PRIO (8)
#define RNGA_POLLER_STACK_SIZE OS_STACK_ALIGN(64)
static struct os_task poller_task;

static inline void
k64f_rnga_start(void)
{
    struct os_event evt;

    RNGA_SetMode(RNG, kRNGA_ModeNormal);
    running = true;

    evt.ev_queued = 0;
    evt.ev_arg = NULL;
    (void)os_eventq_put(&rng_evtq, &evt);
}

static inline void
k64f_rnga_stop(void)
{
   RNGA_SetMode(RNG, kRNGA_ModeSleep);
   running = false;
}

static size_t
k64f_trng_read(struct trng_dev *trng, void *ptr, size_t size)
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
        k64f_rnga_start();
    }

    os_mutex_release(&rng_cache_mu);

    return num_read;
}

static uint32_t
k64f_trng_get_u32(struct trng_dev *trng)
{
    union {
        uint32_t v32;
        uint8_t v8[4];
    } val;
    size_t num;

    num = k64f_trng_read(trng, &val.v8, sizeof(val.v8));
    while (num < sizeof(val.v8)) {
        os_sched(NULL);
        num += k64f_trng_read(trng, &val.v8[num], sizeof(val.v8) - num);
    }

    return val.v32;
}

static void
rnga_poller_handler(void *arg)
{
    int8_t i;
    uint8_t data[4];
    int rc;

    while (1) {
        if (running) {
            rc = RNGA_GetRandomData(RNG, data, sizeof(uint32_t));
            if (rc == kStatus_Success) {
                os_mutex_pend(&rng_cache_mu, OS_TIMEOUT_NEVER);
                for (i = 0; i < 4; i++) {
                    rng_cache[rng_cache_in++] = data[i];

                    if (rng_cache_in >= sizeof(rng_cache)) {
                        rng_cache_in = 0;
                    }

                    if ((rng_cache_in + 1) % sizeof(rng_cache) == rng_cache_out) {
                        k64f_rnga_stop();
                        break;
                    }
                }
                os_mutex_release(&rng_cache_mu);
            }
            os_time_delay(1);
        } else {
            (void)os_eventq_get(&rng_evtq);
        }
    }
}

static int
k64f_trng_dev_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    struct trng_dev *trng;

    trng = (struct trng_dev *)dev;
    assert(trng);

    if (!(dev->od_flags & OS_DEV_F_STATUS_OPEN)) {
        rng_cache_out = 0;
        rng_cache_in = 0;

        RNGA_Init(RNG);

        k64f_rnga_start();
    }

    return 0;
}

int
k64f_trng_dev_init(struct os_dev *dev, void *arg)
{
    struct trng_dev *trng;
    int rc;

    trng = (struct trng_dev *)dev;
    assert(trng);

    OS_DEV_SETHANDLERS(dev, k64f_trng_dev_open, NULL);

    trng->interface.get_u32 = k64f_trng_get_u32;
    trng->interface.read = k64f_trng_read;

    os_eventq_init(&rng_evtq);
    os_mutex_init(&rng_cache_mu);

    pstack = malloc(sizeof(os_stack_t) * RNGA_POLLER_STACK_SIZE);
    assert(pstack);

    rc = os_task_init(&poller_task, "rnga_poller", rnga_poller_handler, NULL,
            RNGA_POLLER_PRIO, OS_WAIT_FOREVER, pstack, RNGA_POLLER_STACK_SIZE);
    if (rc) {
        return rc;
    }

    return 0;
}
