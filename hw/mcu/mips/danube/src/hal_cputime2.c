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
#include <stdint.h>
#include <assert.h>
#include "hal/hal_cputime.h"

/* XXX:
 *  - Must determine how to set priority of cpu timer interrupt
 *  - Determine if we should use a mutex as opposed to disabling interrupts
 *  - Should I use a macro for the timer being used? This is so I can
 *  easily change the timer from 2 to 5? What about compare channel?
 *  - Sync to OSTIME.
 */

void
cputime_disable_ocmp(void)
{
}

/**
 * cputime set ocmp
 *
 * Set the OCMP used by the cputime module to the desired cputime.
 *
 * @param timer Pointer to timer.
 */
void
cputime_set_ocmp(struct cpu_timer *timer)
{
}

/**
 * tim5 isr
 *
 * This is the global timer interrupt routine.
 *
 */
/*static void
cputime_isr(void)
{
}
*/
/**
 * cputime hw init
 *
 * Initialize the cputime hw. This should be called only once and should be
 * called before the hardware timer is used.
 *
 * @param clock_freq The desired cputime frequency, in hertz (Hz).
 *
 * @return int 0 on success; -1 on error.
 */
int
cputime_hw_init(uint32_t clock_freq)
{
    return 0;
}

/**
 * cputime get64
 *
 * Returns cputime as a 64-bit number.
 *
 * @return uint64_t The 64-bit representation of cputime.
 */
uint64_t
cputime_get64(void)
{
    return 0;
}

/**
 * cputime get32
 *
 * Returns the low 32 bits of cputime.
 *
 * @return uint32_t The lower 32 bits of cputime
 */
uint32_t
cputime_get32(void)
{
    return 0;
}
