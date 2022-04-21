/*
 * Copyright (c) 2021 - 2022, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <helpers/nrfx_flag32_allocator.h>

#if !defined(NRFX_ATOMIC_CAS)
static bool nrfx_flag32_atomic_cas(nrfx_atomic_t * p_data, uint32_t old_value, uint32_t new_value)
{
    bool status = false;
    NRFX_CRITICAL_SECTION_ENTER();
    if (*p_data == old_value)
    {
        *p_data = new_value;
        status = true;
    }
    NRFX_CRITICAL_SECTION_EXIT();
    return status;
}

#define NRFX_ATOMIC_CAS(p_data, old_value, new_value) \
    nrfx_flag32_atomic_cas(p_data, old_value, new_value)
#endif // !defined(NRFX_ATOMIC_CAS)

bool nrfx_flag32_is_allocated(nrfx_atomic_t mask, uint8_t bitpos)
{
    return (mask & NRFX_BIT(bitpos)) ? false : true;
}

nrfx_err_t nrfx_flag32_alloc(nrfx_atomic_t * p_mask, uint8_t *p_flag)
{
    int8_t idx;
    uint32_t new_mask, prev_mask;

    do {
        prev_mask = *p_mask;
        idx = 31 - NRF_CLZ(prev_mask);
        if (idx < 0) {
            return NRFX_ERROR_NO_MEM;
        }

        new_mask = prev_mask & ~NRFX_BIT(idx);
    } while (!NRFX_ATOMIC_CAS(p_mask, prev_mask, new_mask));

    *p_flag = idx;

    return NRFX_SUCCESS;
}

nrfx_err_t nrfx_flag32_free(nrfx_atomic_t * p_mask, uint8_t flag)
{
    uint32_t new_mask, prev_mask;

    if ((NRFX_BIT(flag) & *p_mask))
    {
        return NRFX_ERROR_INVALID_PARAM;
    }

    do {
        prev_mask = *p_mask;
        new_mask = prev_mask | NRFX_BIT(flag);
    } while (!NRFX_ATOMIC_CAS(p_mask, prev_mask, new_mask));

    return NRFX_SUCCESS;
}
