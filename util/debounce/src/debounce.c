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

#include "os/mynewt.h"
#include "debounce/debounce.h"

void
debouncer_set(struct debouncer *debouncer, uint16_t val)
{
    debouncer->cur = val;

    if (debouncer->state) {
        if (debouncer->cur <= debouncer->thresh_low) {
            debouncer->state = 0;
        }
    } else {
        if (debouncer->cur >= debouncer->thresh_high) {
            debouncer->state = 1;
        }
    }
}

int
debouncer_adjust(struct debouncer *debouncer, int32_t delta)
{
    int32_t new_val;

    if (delta > UINT16_MAX || delta < -UINT16_MAX) {
        return SYS_EINVAL;
    }

    new_val = debouncer->cur + delta;
    if (new_val > debouncer->max) {
        new_val = debouncer->max;
    } else if (new_val < 0) {
        new_val = 0;
    }

    debouncer_set(debouncer, new_val);

    return 0;
}

void
debouncer_reset(struct debouncer *debouncer)
{
    debouncer->cur = 0;
    debouncer->state = 0;
}

int
debouncer_init(struct debouncer *debouncer, uint16_t thresh_low,
               uint16_t thresh_high, uint16_t max)
{
    if (thresh_low >= thresh_high) {
        return SYS_EINVAL;
    }

    if (thresh_high > max) {
        return SYS_EINVAL;
    }

    *debouncer = (struct debouncer) {
        .thresh_low = thresh_low,
        .thresh_high = thresh_high,
        .max = max,
        .cur = 0,
        .state = 0,
    };

    return 0;
}
