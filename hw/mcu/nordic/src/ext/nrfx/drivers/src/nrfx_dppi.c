/*
 * Copyright (c) 2018 - 2022, Nordic Semiconductor ASA
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

#include <nrfx.h>

#if NRFX_CHECK(NRFX_DPPI_ENABLED)

#include <nrfx_dppi.h>
#include <helpers/nrfx_flag32_allocator.h>

#define NRFX_LOG_MODULE DPPI
#include <nrfx_log.h>

#if !defined(NRFX_DPPI_CHANNELS_USED)
// Default mask of DPPI channels reserved for other modules.
#define NRFX_DPPI_CHANNELS_USED 0x00000000uL
#endif

#if !defined(NRFX_DPPI_GROUPS_USED)
// Default mask of DPPI groups reserved for other modules.
#define NRFX_DPPI_GROUPS_USED 0x00000000uL
#endif

#define DPPI_AVAILABLE_CHANNELS_MASK \
    ((uint32_t)(((1ULL << DPPI_CH_NUM) - 1) & (~NRFX_DPPI_CHANNELS_USED)))

#define DPPI_AVAILABLE_GROUPS_MASK   \
    (((1UL << DPPI_GROUP_NUM) - 1)   & (~NRFX_DPPI_GROUPS_USED))

/** @brief Set bit at given position. */
#define DPPI_BIT_SET(pos) (1uL << (pos))

/**< Bitmap representing channels availability. */
static nrfx_atomic_t   m_allocated_channels = DPPI_AVAILABLE_CHANNELS_MASK;
/**< Bitmap representing groups availability. */
static nrfx_atomic_t   m_allocated_groups = DPPI_AVAILABLE_GROUPS_MASK;

void nrfx_dppi_free(void)
{
    uint32_t mask = DPPI_AVAILABLE_GROUPS_MASK & ~m_allocated_groups;
    uint8_t group_idx = NRF_DPPI_CHANNEL_GROUP0;

    // Disable all channels
    nrf_dppi_channels_disable(NRF_DPPIC,
                              DPPI_AVAILABLE_CHANNELS_MASK & ~m_allocated_channels);

    // Clear all groups configurations
    while (mask)
    {
        nrf_dppi_channel_group_t group = (nrf_dppi_channel_group_t)group_idx;
        if (mask & DPPI_BIT_SET(group))
        {
            nrf_dppi_group_clear(NRF_DPPIC, group);
            mask &= ~DPPI_BIT_SET(group);
        }
        group_idx++;
    }

    // Clear all allocated channels.
    m_allocated_channels = DPPI_AVAILABLE_CHANNELS_MASK;

    // Clear all allocated groups.
    m_allocated_groups = DPPI_AVAILABLE_GROUPS_MASK;
}

nrfx_err_t nrfx_dppi_channel_alloc(uint8_t * p_channel)
{
    return nrfx_flag32_alloc(&m_allocated_channels, p_channel);
}

nrfx_err_t nrfx_dppi_channel_free(uint8_t channel)
{
    nrf_dppi_channels_disable(NRF_DPPIC, NRFX_BIT(channel));
    return nrfx_flag32_free(&m_allocated_channels, channel);
}

nrfx_err_t nrfx_dppi_channel_enable(uint8_t channel)
{
    nrfx_err_t err_code = NRFX_SUCCESS;

    if (!nrfx_flag32_is_allocated(m_allocated_channels, channel))
    {
        err_code = NRFX_ERROR_INVALID_PARAM;
    }
    else
    {
        nrf_dppi_channels_enable(NRF_DPPIC, DPPI_BIT_SET(channel));
    }
    NRFX_LOG_INFO("Function: %s, error code: %s.", __func__, NRFX_LOG_ERROR_STRING_GET(err_code));
    return err_code;
}

nrfx_err_t nrfx_dppi_channel_disable(uint8_t channel)
{
    nrfx_err_t err_code = NRFX_SUCCESS;

    if (!nrfx_flag32_is_allocated(m_allocated_channels, channel))
    {
        err_code = NRFX_ERROR_INVALID_PARAM;
    }
    else
    {
        nrf_dppi_channels_disable(NRF_DPPIC, DPPI_BIT_SET(channel));
        err_code = NRFX_SUCCESS;
    }
    NRFX_LOG_INFO("Function: %s, error code: %s.", __func__, NRFX_LOG_ERROR_STRING_GET(err_code));
    return err_code;
}

nrfx_err_t nrfx_dppi_group_alloc(nrf_dppi_channel_group_t * p_group)
{
    return nrfx_flag32_alloc(&m_allocated_groups, (uint8_t *)p_group);
}

nrfx_err_t nrfx_dppi_group_free(nrf_dppi_channel_group_t group)
{
    nrf_dppi_group_disable(NRF_DPPIC, group);
    return nrfx_flag32_free(&m_allocated_groups, group);
}

nrfx_err_t nrfx_dppi_channel_include_in_group(uint8_t                  channel,
                                              nrf_dppi_channel_group_t group)
{
    nrfx_err_t err_code = NRFX_SUCCESS;

    if (!nrfx_flag32_is_allocated(m_allocated_groups, group) ||
        !nrfx_flag32_is_allocated(m_allocated_channels, channel))
    {
        err_code = NRFX_ERROR_INVALID_PARAM;
    }
    else
    {
        NRFX_CRITICAL_SECTION_ENTER();
        nrf_dppi_channels_include_in_group(NRF_DPPIC, DPPI_BIT_SET(channel), group);
        NRFX_CRITICAL_SECTION_EXIT();
    }
    NRFX_LOG_INFO("Function: %s, error code: %s.", __func__, NRFX_LOG_ERROR_STRING_GET(err_code));
    return err_code;
}

nrfx_err_t nrfx_dppi_channel_remove_from_group(uint8_t                  channel,
                                               nrf_dppi_channel_group_t group)
{
    nrfx_err_t err_code = NRFX_SUCCESS;

    if (!nrfx_flag32_is_allocated(m_allocated_groups, group) ||
        !nrfx_flag32_is_allocated(m_allocated_channels, channel))
    {
        err_code = NRFX_ERROR_INVALID_PARAM;
    }
    else
    {
        NRFX_CRITICAL_SECTION_ENTER();
        nrf_dppi_channels_remove_from_group(NRF_DPPIC, DPPI_BIT_SET(channel), group);
        NRFX_CRITICAL_SECTION_EXIT();
    }
    NRFX_LOG_INFO("Function: %s, error code: %s.", __func__, NRFX_LOG_ERROR_STRING_GET(err_code));
    return err_code;
}

nrfx_err_t nrfx_dppi_group_clear(nrf_dppi_channel_group_t group)
{
    nrfx_err_t err_code = NRFX_SUCCESS;

    if (!nrfx_flag32_is_allocated(m_allocated_groups, group))
    {
        err_code = NRFX_ERROR_INVALID_PARAM;
    }
    else
    {
        nrf_dppi_channels_remove_from_group(NRF_DPPIC, DPPI_AVAILABLE_CHANNELS_MASK, group);
    }
    NRFX_LOG_INFO("Function: %s, error code: %s.", __func__, NRFX_LOG_ERROR_STRING_GET(err_code));
    return err_code;
}

nrfx_err_t nrfx_dppi_group_enable(nrf_dppi_channel_group_t group)
{
    nrfx_err_t err_code = NRFX_SUCCESS;

    if (!nrfx_flag32_is_allocated(m_allocated_groups, group))
    {
        err_code = NRFX_ERROR_INVALID_PARAM;
    }
    else
    {
        nrf_dppi_group_enable(NRF_DPPIC, group);
    }
    NRFX_LOG_INFO("Function: %s, error code: %s.", __func__, NRFX_LOG_ERROR_STRING_GET(err_code));
    return err_code;
}

nrfx_err_t nrfx_dppi_group_disable(nrf_dppi_channel_group_t group)
{
    nrfx_err_t err_code = NRFX_SUCCESS;

    if (!nrfx_flag32_is_allocated(m_allocated_groups, group))
    {
        err_code = NRFX_ERROR_INVALID_PARAM;
    }
    else
    {
        nrf_dppi_group_disable(NRF_DPPIC, group);
    }
    NRFX_LOG_INFO("Function: %s, error code: %s.", __func__, NRFX_LOG_ERROR_STRING_GET(err_code));
    return err_code;
}

#endif // NRFX_CHECK(NRFX_DPPI_ENABLED)
