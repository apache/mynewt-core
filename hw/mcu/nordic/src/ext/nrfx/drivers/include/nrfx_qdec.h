/*
 * Copyright (c) 2015 - 2022, Nordic Semiconductor ASA
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

#ifndef NRFX_QDEC_H__
#define NRFX_QDEC_H__

#include <nrfx.h>
#include <hal/nrf_qdec.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup nrfx_qdec QDEC driver
 * @{
 * @ingroup nrf_qdec
 * @brief   Quadrature Decoder (QDEC) peripheral driver.
 */

/** @brief QDEC configuration structure. */
typedef struct
{
    nrf_qdec_reportper_t reportper;          ///< Report period in samples.
    nrf_qdec_sampleper_t sampleper;          ///< Sampling period in microseconds.
    uint32_t             psela;              ///< Pin number for A input.
    uint32_t             pselb;              ///< Pin number for B input.
    uint32_t             pselled;            ///< Pin number for LED output.
    uint32_t             ledpre;             ///< Time (in microseconds) how long LED is switched on before sampling.
    nrf_qdec_ledpol_t    ledpol;             ///< Active LED polarity.
    bool                 dbfen;              ///< State of debouncing filter.
    bool                 sample_inten;       ///< Enabling sample ready interrupt.
    uint8_t              interrupt_priority; ///< QDEC interrupt priority.
    bool                 skip_gpio_cfg;      ///< Skip GPIO configuration of pins.
                                             /**< When set to true, the driver does not modify
                                              *   any GPIO parameters of the used pins. Those
                                              *   parameters are supposed to be configured
                                              *   externally before the driver is initialized. */
    bool                 skip_psel_cfg;      ///< Skip pin selection configuration.
                                             /**< When set to true, the driver does not modify
                                              *   pin select registers in the peripheral.
                                              *   Those registers are supposed to be set up
                                              *   externally before the driver is initialized.
                                              *   @note When both GPIO configuration and pin
                                              *   selection are to be skipped, the structure
                                              *   fields that specify pins can be omitted,
                                              *   as they are ignored anyway. */
} nrfx_qdec_config_t;

/**
 * @brief QDEC driver default configuration.
 *
 * This configuration sets up QDEC with the following options:
 * - report period: 10 samples
 * - sampling period: 16384 us
 * - LED enabled for 500 us before sampling
 * - LED polarity: active high
 * - debouncing filter disabled
 * - sample ready interrupt disabled
 *
 * @param[in] _pin_a   Pin for A encoder channel input.
 * @param[in] _pin_b   Pin for B encoder channel input.
 * @param[in] _pin_led Pin for LED output.
 */
#define NRFX_QDEC_DEFAULT_CONFIG(_pin_a, _pin_b, _pin_led)           \
    {                                                                \
        .reportper          = NRF_QDEC_REPORTPER_10,                 \
        .sampleper          = NRF_QDEC_SAMPLEPER_16384us,            \
        .psela              = _pin_a,                                \
        .pselb              = _pin_b,                                \
        .pselled            = _pin_led,                              \
        .ledpre             = 500,                                   \
        .ledpol             = NRF_QDEC_LEPOL_ACTIVE_HIGH,            \
        .dbfen              = NRF_QDEC_DBFEN_DISABLE,                \
        .sample_inten       = false,                                 \
        .interrupt_priority = NRFX_QDEC_DEFAULT_CONFIG_IRQ_PRIORITY  \
    }

/** @brief QDEC sample event data. */
typedef struct
{
    int8_t value; /**< Sample value. */
} nrfx_qdec_sample_data_evt_t;

/** @brief QDEC report event data. */
typedef struct
{
    int16_t acc;     /**< Accumulated transitions. */
    uint16_t accdbl; /**< Accumulated double transitions. */
} nrfx_qdec_report_data_evt_t;

/** @brief QDEC event handler structure. */
typedef struct
{
    nrf_qdec_event_t  type; /**< Event type. */
    union
    {
        nrfx_qdec_sample_data_evt_t sample; /**< Sample event data. */
        nrfx_qdec_report_data_evt_t report; /**< Report event data. */
    } data;                                 /**< Union to store event data. */
} nrfx_qdec_event_t;

/**
 * @brief QDEC event handler.
 *
 * @param[in] event QDEC event structure.
 */
typedef void (*nrfx_qdec_event_handler_t)(nrfx_qdec_event_t event);

/**
 * @brief Function for initializing QDEC.
 *
 * @param[in] p_config      Pointer to the structure with the initial configuration.
 * @param[in] event_handler Event handler provided by the user.
 *                          Must not be NULL.
 *
 * @retval NRFX_SUCCESS             Initialization was successful.
 * @retval NRFX_ERROR_INVALID_STATE The QDEC was already initialized.
 */
nrfx_err_t nrfx_qdec_init(nrfx_qdec_config_t const * p_config,
                          nrfx_qdec_event_handler_t  event_handler);

/**
 * @brief Function for uninitializing QDEC.
 *
 * @note Function asserts if module is uninitialized.
 */
void nrfx_qdec_uninit(void);

/**
 * @brief Function for enabling QDEC.
 *
 * @note Function asserts if module is uninitialized or enabled.
 */
void nrfx_qdec_enable(void);

/**
 * @brief Function for disabling QDEC.
 *
 * @note Function asserts if module is uninitialized or disabled.
 */
void nrfx_qdec_disable(void);

/**
 * @brief Function for reading accumulated transitions from the QDEC peripheral.
 *
 * @note Function asserts if module is not enabled.
 * @note Accumulators are cleared after reading.
 *
 * @param[out] p_acc    Pointer to store the accumulated transitions.
 * @param[out] p_accdbl Pointer to store the accumulated double transitions.
 */
void nrfx_qdec_accumulators_read(int16_t * p_acc, int16_t * p_accdbl);

/**
 * @brief Function for returning the address of the specified QDEC task.
 *
 * @param task QDEC task.
 *
 * @return Task address.
 */
NRFX_STATIC_INLINE uint32_t nrfx_qdec_task_address_get(nrf_qdec_task_t task);

/**
 * @brief Function for returning the address of the specified QDEC event.
 *
 * @param event QDEC event.
 *
 * @return Event address.
 */
NRFX_STATIC_INLINE uint32_t nrfx_qdec_event_address_get(nrf_qdec_event_t event);

#ifndef NRFX_DECLARE_ONLY
NRFX_STATIC_INLINE uint32_t nrfx_qdec_task_address_get(nrf_qdec_task_t task)
{
    return nrf_qdec_task_address_get(NRF_QDEC, task);
}

NRFX_STATIC_INLINE uint32_t nrfx_qdec_event_address_get(nrf_qdec_event_t event)
{
    return nrf_qdec_event_address_get(NRF_QDEC, event);
}
#endif // NRFX_DECLARE_ONLY

/** @} */


void nrfx_qdec_irq_handler(void);


#ifdef __cplusplus
}
#endif

#endif // NRFX_QDEC_H__
