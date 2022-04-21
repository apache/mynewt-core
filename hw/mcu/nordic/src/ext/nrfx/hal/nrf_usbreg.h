/*
 * Copyright (c) 2019 - 2022, Nordic Semiconductor ASA
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

#ifndef NRF_USBREG_H__
#define NRF_USBREG_H__

#include <nrfx.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup nrf_usbreg_hal USBREG HAL
 * @{
 * @ingroup nrf_usbd
 * @ingroup nrf_power
 * @brief   Hardware access layer for managing the USB regulator peripheral.
 */

/** @brief USBREG events. */
typedef enum
{
    NRF_USBREG_EVENT_USBDETECTED = offsetof(NRF_USBREG_Type, EVENTS_USBDETECTED), /**< Voltage supply detected on VBUS. */
    NRF_USBREG_EVENT_USBREMOVED  = offsetof(NRF_USBREG_Type, EVENTS_USBREMOVED),  /**< Voltage supply removed from VBUS. */
    NRF_USBREG_EVENT_USBPWRRDY   = offsetof(NRF_USBREG_Type, EVENTS_USBPWRRDY)    /**< USB 3.3&nbsp;V supply ready. */
} nrf_usbreg_event_t;

/** @brief USBREG interrupts. */
typedef enum
{
    NRF_USBREG_INT_USBDETECTED = USBREG_INTEN_USBDETECTED_Msk, /**< Interrupt on USBDETECTED. */
    NRF_USBREG_INT_USBREMOVED  = USBREG_INTEN_USBREMOVED_Msk,  /**< Interrupt on USBREMOVED. */
    NRF_USBREG_INT_USBPWRRDY   = USBREG_INTEN_USBPWRRDY_Msk    /**< Interrupt on USBPWRRDY. */
} nrf_usbreg_int_mask_t;

/** @brief USBREGSTATUS register bit masks. */
typedef enum
{
    NRF_USBREG_STATUS_VBUSDETECT_MASK = USBREG_USBREGSTATUS_VBUSDETECT_Msk, /**< USB detected or removed.     */
    NRF_USBREG_STATUS_OUTPUTRDY_MASK  = USBREG_USBREGSTATUS_OUTPUTRDY_Msk   /**< USB 3.3&nbsp;V supply ready. */
} nrf_usbreg_status_mask_t;

/**
 * @brief Function for clearing the specified USBREG event.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] event Event to be cleared.
 */
NRF_STATIC_INLINE void nrf_usbreg_event_clear(NRF_USBREG_Type *  p_reg,
                                              nrf_usbreg_event_t event);

/**
 * @brief Function for retrieving the state of the USBREG event.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] event Event to be checked.
 *
 * @retval true  The event has been generated.
 * @retval false The event has not been generated.
 */
NRF_STATIC_INLINE bool nrf_usbreg_event_check(NRF_USBREG_Type const * p_reg,
                                              nrf_usbreg_event_t      event);

/**
 * @brief Function for enabling specified interrupts.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] mask  Mask of interrupts to be enabled.
 */
NRF_STATIC_INLINE void nrf_usbreg_int_enable(NRF_USBREG_Type * p_reg, uint32_t mask);

/**
 * @brief Function for disabling specified interrupts.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] mask  Mask of interrupts to be disabled.
 */
NRF_STATIC_INLINE void nrf_usbreg_int_disable(NRF_USBREG_Type * p_reg, uint32_t mask);

/**
 * @brief Function for checking if the specified interrupts are enabled.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] mask  Mask of interrupts to be checked.
 *
 * @return Mask of enabled interrupts.
 */
NRF_STATIC_INLINE uint32_t nrf_usbreg_int_enable_check(NRF_USBREG_Type const * p_reg,
                                                       uint32_t                mask);

/**
 * @brief Function for getting the whole USBREGSTATUS register.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 *
 * @return The USBREGSTATUS register value.
 *         Use @ref nrf_usbreg_status_mask_t values for bit masking.
 */
NRF_STATIC_INLINE uint32_t nrf_usbreg_status_get(NRF_USBREG_Type const * p_reg);

#ifndef NRF_DECLARE_ONLY

NRF_STATIC_INLINE void nrf_usbreg_event_clear(NRF_USBREG_Type *  p_reg,
                                              nrf_usbreg_event_t event)
{
    *((volatile uint32_t *)((uint8_t *)p_reg + (uint32_t)event)) = 0x0UL;
}

NRF_STATIC_INLINE bool nrf_usbreg_event_check(NRF_USBREG_Type const * p_reg,
                                              nrf_usbreg_event_t      event)
{
    return (bool)*(volatile uint32_t *)((uint8_t *)p_reg + (uint32_t)event);
}

NRF_STATIC_INLINE void nrf_usbreg_int_enable(NRF_USBREG_Type * p_reg, uint32_t mask)
{
    p_reg->INTENSET = mask;
}

NRF_STATIC_INLINE void nrf_usbreg_int_disable(NRF_USBREG_Type * p_reg, uint32_t mask)
{
    p_reg->INTENCLR = mask;
}

NRF_STATIC_INLINE uint32_t nrf_usbreg_int_enable_check(NRF_USBREG_Type const * p_reg,
                                                       uint32_t                mask)
{
    return p_reg->INTENSET & mask;
}

NRF_STATIC_INLINE uint32_t nrf_usbreg_status_get(NRF_USBREG_Type const * p_reg)
{
    return p_reg->USBREGSTATUS;
}

#endif // NRF_DECLARE_ONLY

/** @} */

#ifdef __cplusplus
}
#endif

#endif // NRF_USBREG_H__
