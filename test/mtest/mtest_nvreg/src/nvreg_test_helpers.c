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

#include "hal/hal_nvreg.h"
#include "mtest_nvreg/mtest_nvreg.h"

extern int reset_count;

void
nvregs_reset(void)
{
    reset_count++;
    /* Ensure logs are flushed before reset */
    os_time_delay(OS_TICKS_PER_SEC / 30);
    hal_system_reset();
}

void
nvregs_set_zero(void)
{
    int i;

    for (i = 0; i < hal_nvreg_get_num_regs(); i++) {
        hal_nvreg_write(i, 0);
    }
}

int
nvregs_compare_zero(struct nvreg_err_dump *ed)
{
    uint32_t nvreg_val;
    int i;

    for (i = 0; i < hal_nvreg_get_num_regs(); i++) {
        nvreg_val = hal_nvreg_read(i);
        if (nvreg_val != 0) {
            *ed = (struct nvreg_err_dump){ .reg = i, .val = nvreg_val, .exp_val = 0 };
            return 1;
        }
    }
    return 0;
}

void
nvreg_set_pattern(void)
{
    uint32_t set_val;
    uint32_t max_bits;
    int i;

    max_bits = hal_nvreg_get_reg_width() * 8;
    for (i = 0; i < hal_nvreg_get_num_regs(); i++) {
        set_val = 1 << (i % max_bits);
        hal_nvreg_write(i, set_val);
    }
}

int
nvregs_compare_pattern(struct nvreg_err_dump *ed)
{
    uint32_t max_bits;
    uint32_t nvreg_val;
    uint32_t exp_val;
    int i;

    max_bits = hal_nvreg_get_reg_width() * 8;
    for (i = 0; i < hal_nvreg_get_num_regs(); i++) {
        exp_val = 1 << (i % max_bits);
        nvreg_val = hal_nvreg_read(i);
        if (nvreg_val != exp_val) {
            *ed = (struct nvreg_err_dump){ .reg = i, .val = nvreg_val, .exp_val = exp_val };
            return 1;
        }
    }
    return 0;
}
