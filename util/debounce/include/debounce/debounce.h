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

#ifndef H_DEBOUNCE_
#define H_DEBOUNCE_

#include <inttypes.h>
#include <stdbool.h>
#include "os/mynewt.h"

/**
 * @brief Debouncer - toggles between two states with jitter control.
 *
 * A debouncer is always in one of two states: low or high.  The state is
 * derived from changes to the debouncer's counter.  When the counter increases
 * up to the debouncer's high threshold (>=), the debouncer enters the high state.
 * The debouner remains in the high state until its counter drops down to its
 * low threshold (<=).  The counter saturates at a configured maximum value
 * (i.e., the counter will never exceed the max).
 *
 * Restrictions:
 *     o thresh_low < thresh_high
 *     o thresh_high <= max
 * 
 * All struct fields should be considered private.
 */
struct debouncer {
    uint16_t thresh_low;
    uint16_t thresh_high;
    uint16_t max;
    uint16_t cur;
    uint8_t state;
};

/**
 * @brief Sets the provided debouncer's counter to the specified value.
 *
 * @param debouncer             The debouncer to set.
 * @param val                   The value to set the debouncer's counter to.
 */
void debouncer_set(struct debouncer *debouncer, uint16_t val);

/**
 * @brief Adjusts the provided debouncer's counter by the specified amonut.
 *
 * @param debouncer             The debouncer to adjust.
 * @param delta                 The amount to change the debouncer's counter by
 *                                  (positive for increase; negative for
 *                                  decrease).
 *
 * @return                      0 on success;
 *                              SYS_EINVAL if the specified delta is out of
 *                                  range (less than -UINT16_MAX or greater
 *                                  than UINT16_MAX)
 */
int debouncer_adjust(struct debouncer *debouncer, int32_t delta);

/**
 * @brief Initializes a debouncer with the specified configuration.
 *
 * @param debouncer             The debouncer to initialize.
 * @param thresh_low            The low threshold.
 * @param thresh_high           The high threshold.
 * @param max                   The maximum value the debouncer's counter can
 *                                  attain.
 *
 * @return                      0 on success; SYS_EINVAL on error
 */
int debouncer_init(struct debouncer *debouncer, uint16_t thresh_low,
                   uint16_t thresh_high, uint16_t max);

/**
 * @brief Indicates which of the two states the provided debouncer is in.
 *
 * @param debouncer             The debouncer to query.
 *
 * @return                      1 if the debouncer is in the high state;
 *                              0 if the debouncer is in the low state.
 */
static inline int
debouncer_state(const struct debouncer *debouncer)
{
    return debouncer->state;
}

/**
 * @brief Retrieves the provided debouncer's current counter value.
 *
 * @param debouncer             The debouncer to query.
 *
 * @return                      The debouncer's counter value.
 */
static inline uint16_t
debouncer_val(const struct debouncer *debouncer)
{
    return debouncer->cur;
}

#endif
