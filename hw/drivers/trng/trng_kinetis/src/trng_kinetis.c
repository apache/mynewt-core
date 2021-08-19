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
#define TRNG_CLEAR_AND_ENABLE_INTS(base)                              \
    do {                                                              \
        (base)->INT_CTRL &= ~(TRNG_INT_CTRL_HW_ERR_MASK |             \
                              TRNG_INT_CTRL_ENT_VAL_MASK);            \
        (base)->INT_MASK |= TRNG_INT_MASK_HW_ERR_MASK |               \
                            TRNG_INT_MASK_ENT_VAL_MASK;               \
    } while (0)
#define TRNG_CLEAR_AND_ENABLE_ENTROPY_INT(base)                       \
    do {                                                              \
        (base)->INT_CTRL &= ~TRNG_INT_CTRL_ENT_VAL_MASK;              \
        (base)->INT_MASK |= TRNG_INT_MASK_ENT_VAL_MASK;               \
    } while (0)
#define TRNG_DISABLE_ENTROPY_INT(base)                                \
    do {                                                              \
        (base)->INT_MASK &= ~TRNG_INT_MASK_ENT_VAL_MASK;              \
    } while (0)
#else
#error "Unsupported TRNG interface"
#endif

#include "trng/trng.h"
#include "trng_kinetis/trng_kinetis.h"

#define TRNG_CACHE_LEN MYNEWT_VAL(KINETIS_TRNG_CACHE_LEN)
static struct {
    uint16_t out;
    uint16_t in;
    uint16_t used;
    struct os_mutex mu;
    uint8_t cache[TRNG_CACHE_LEN];
} rng_state;
static_assert(sizeof(rng_state.cache) == TRNG_CACHE_LEN,
              "Must fix TRNG_CACHE_LEN usage");
#define CACHE_OUT(x)                                               \
    do {                                                           \
        (x) = rng_state.cache[rng_state.out];                      \
        rng_state.out = (rng_state.out + 1) % TRNG_CACHE_LEN;      \
        rng_state.used--;                                          \
    } while (0)
#define CACHE_IN(x)                                                \
    do {                                                           \
        rng_state.cache[rng_state.in] = (x);                       \
        rng_state.in = (rng_state.in + 1) % TRNG_CACHE_LEN;        \
        rng_state.used++;                                          \
    } while (0)
#define IS_CACHE_FULL() (rng_state.used == TRNG_CACHE_LEN)
#define IS_CACHE_EMPTY() (rng_state.used == 0)
#define CACHE_INIT()                                               \
    do {                                                           \
        rng_state.out = 0;                                         \
        rng_state.in = 0;                                          \
        rng_state.used = 0;                                        \
    } while (0)
#define CACHE_LOCK() os_mutex_pend(&rng_state.mu, OS_TIMEOUT_NEVER)
#define CACHE_UNLOCK() os_mutex_release(&rng_state.mu)
#define CACHE_INIT_LOCK() os_mutex_init(&rng_state.mu)

#if USE_RNGA
static bool running;
#endif
static struct os_eventq rng_evtq;
static struct os_event evt = {0};

#define TRNG_POLLER_PRIO (8)
#define TRNG_POLLER_STACK_SIZE OS_STACK_ALIGN(64)
static struct os_task trng_poller_task;
OS_TASK_STACK_DEFINE(trng_poller_stack, TRNG_POLLER_STACK_SIZE);

#if USE_TRNG
static void
trng_irq_handler(void)
{
    if (TRNG0->MCTL & TRNG_MCTL_ERR_MASK) {
        TRNG0->MCTL |= TRNG_MCTL_ERR_MASK;
    }

    if (TRNG0->INT_CTRL & TRNG_INT_CTRL_HW_ERR_MASK) {
        TRNG0->INT_CTRL &= ~TRNG_INT_CTRL_HW_ERR_MASK;
    }

    if (TRNG0->INT_CTRL & TRNG_INT_CTRL_ENT_VAL_MASK) {
        TRNG_DISABLE_ENTROPY_INT(TRNG0);
        (void)os_eventq_put(&rng_evtq, &evt);
    }
}
#endif

static void
kinetis_trng_start(void)
{
#if USE_RNGA
    RNGA_SetMode(RNG, kRNGA_ModeNormal);
    running = true;
    (void)os_eventq_put(&rng_evtq, &evt);
#elif USE_TRNG
    TRNG_START(TRNG0);
#endif
}

static inline void
kinetis_trng_stop(void)
{
#if USE_RNGA
    RNGA_SetMode(RNG, kRNGA_ModeSleep);
    running = false;
#elif USE_TRNG
    TRNG_STOP(TRNG0);
#endif
}

static size_t
kinetis_trng_read(struct trng_dev *trng, void *ptr, size_t size)
{
    size_t num_read;
    uint8_t *u8p;

    num_read = 0;
    u8p = (uint8_t *)ptr;

    CACHE_LOCK();

    while (!IS_CACHE_EMPTY() && size) {
        CACHE_OUT(*u8p++);
        num_read++;
        size--;
    }

    CACHE_UNLOCK();

    if (num_read > 0) {
        kinetis_trng_start();
    }

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
#if USE_TRNG
        (void)os_eventq_get(&rng_evtq);
#endif

#if USE_RNGA
        if (running) {
            rc = RNGA_GetRandomData(RNG, data, sizeof(uint32_t));
#else
            rc = TRNG_GetRandomData(TRNG0, data, sizeof(uint32_t));
#endif
            if (rc == 0) {
                CACHE_LOCK();

                for (i = 0; i < 4; i++) {
                    if (IS_CACHE_FULL()) {
                        kinetis_trng_stop();
                        break;
                    }

                    CACHE_IN(data[i]);
                }

                CACHE_UNLOCK();

#if USE_TRNG
                TRNG_CLEAR_AND_ENABLE_ENTROPY_INT(TRNG0);
#endif
            }
#if USE_RNGA
        } else {
            (void)os_eventq_get(&rng_evtq);
        }
#endif
    }
}

static int
kinetis_trng_dev_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    struct trng_dev *trng;
#if USE_TRNG
    trng_config_t trng_config;
#endif

    trng = (struct trng_dev *)dev;
    assert(trng);

    if (!(dev->od_flags & OS_DEV_F_STATUS_OPEN)) {
        CACHE_INIT();

#if USE_RNGA
        RNGA_Init(RNG);
        RNGA_Seed(RNG, SIM->UIDL);

#elif USE_TRNG
        NVIC_SetVector(TRNG0_IRQn, (uint32_t)trng_irq_handler);
        NVIC_EnableIRQ(TRNG0_IRQn);

        (void)TRNG_GetDefaultConfig(&trng_config);
        trng_config.entropyDelay = MYNEWT_VAL(KINETIS_TRNG_ENTROPY_DELAY);
        TRNG_Init(TRNG0, &trng_config);

        TRNG_CLEAR_AND_ENABLE_INTS(TRNG0);
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
    CACHE_INIT_LOCK();

    rc = os_task_init(&trng_poller_task, "trng_poller", trng_poller_handler, NULL,
            TRNG_POLLER_PRIO, OS_WAIT_FOREVER, trng_poller_stack,
            TRNG_POLLER_STACK_SIZE);
    if (rc) {
        return rc;
    }

    return 0;
}
