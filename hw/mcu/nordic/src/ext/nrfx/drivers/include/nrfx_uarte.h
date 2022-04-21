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

#ifndef NRFX_UARTE_H__
#define NRFX_UARTE_H__

#include <nrfx.h>
#include <hal/nrf_uarte.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup nrfx_uarte UARTE driver
 * @{
 * @ingroup nrf_uarte
 * @brief   UARTE peripheral driver.
 */

/** @brief Structure for the UARTE driver instance. */
typedef struct
{
    NRF_UARTE_Type * p_reg;        ///< Pointer to a structure with UARTE registers.
    uint8_t          drv_inst_idx; ///< Index of the driver instance. For internal use only.
} nrfx_uarte_t;

#ifndef __NRFX_DOXYGEN__
enum {
#if NRFX_CHECK(NRFX_UARTE0_ENABLED)
    NRFX_UARTE0_INST_IDX,
#endif
#if NRFX_CHECK(NRFX_UARTE1_ENABLED)
    NRFX_UARTE1_INST_IDX,
#endif
#if NRFX_CHECK(NRFX_UARTE2_ENABLED)
    NRFX_UARTE2_INST_IDX,
#endif
#if NRFX_CHECK(NRFX_UARTE3_ENABLED)
    NRFX_UARTE3_INST_IDX,
#endif
    NRFX_UARTE_ENABLED_COUNT
};
#endif

/** @brief Macro for creating a UARTE driver instance. */
#define NRFX_UARTE_INSTANCE(id)                               \
{                                                             \
    .p_reg        = NRFX_CONCAT_2(NRF_UARTE, id),             \
    .drv_inst_idx = NRFX_CONCAT_3(NRFX_UARTE, id, _INST_IDX), \
}

/** @brief Types of UARTE driver events. */
typedef enum
{
    NRFX_UARTE_EVT_TX_DONE, ///< Requested TX transfer completed.
    NRFX_UARTE_EVT_RX_DONE, ///< Requested RX transfer completed.
    NRFX_UARTE_EVT_ERROR,   ///< Error reported by UART peripheral.
} nrfx_uarte_evt_type_t;

/** @brief Structure for the UARTE configuration. */
typedef struct
{
    uint32_t             pseltxd;            ///< TXD pin number.
    uint32_t             pselrxd;            ///< RXD pin number.
    uint32_t             pselcts;            ///< CTS pin number.
    uint32_t             pselrts;            ///< RTS pin number.
    void *               p_context;          ///< Context passed to interrupt handler.
    nrf_uarte_baudrate_t baudrate;           ///< Baud rate.
    uint8_t              interrupt_priority; ///< Interrupt priority.
    nrf_uarte_config_t   hal_cfg;            ///< Parity, flow control and stop bits settings.
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
} nrfx_uarte_config_t;

#if defined(UARTE_CONFIG_STOP_Msk) || defined(__NRFX_DOXYGEN__)
    /** @brief UARTE additional stop bits configuration. */
    #define NRFX_UARTE_DEFAULT_EXTENDED_STOP_CONFIG   \
        .stop = (nrf_uarte_stop_t)NRF_UARTE_STOP_ONE,
#else
    #define NRFX_UARTE_DEFAULT_EXTENDED_STOP_CONFIG
#endif

#if defined(UARTE_CONFIG_PARITYTYPE_Msk) || defined(__NRFX_DOXYGEN__)
    /** @brief UARTE additional parity type configuration. */
    #define NRFX_UARTE_DEFAULT_EXTENDED_PARITYTYPE_CONFIG   \
        .paritytype = NRF_UARTE_PARITYTYPE_EVEN,
#else
    #define NRFX_UARTE_DEFAULT_EXTENDED_PARITYTYPE_CONFIG
#endif

/**
 * @brief UARTE driver default configuration.
 *
 * This configuration sets up UARTE with the following options:
 * - hardware flow control disabled
 * - no parity bit
 * - one stop bit
 * - baudrate: 115200
 *
 * @param[in] _pin_tx TX pin.
 * @param[in] _pin_rx RX pin.
 */
#define NRFX_UARTE_DEFAULT_CONFIG(_pin_tx, _pin_rx)                                 \
{                                                                                   \
    .pseltxd            = _pin_tx,                                                  \
    .pselrxd            = _pin_rx,                                                  \
    .pselcts            = NRF_UARTE_PSEL_DISCONNECTED,                              \
    .pselrts            = NRF_UARTE_PSEL_DISCONNECTED,                              \
    .p_context          = NULL,                                                     \
    .baudrate           = NRF_UARTE_BAUDRATE_115200,                                \
    .interrupt_priority = NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY,                   \
    .hal_cfg            = {                                                         \
        .hwfc           = NRF_UARTE_HWFC_DISABLED,                                  \
        .parity         = NRF_UARTE_PARITY_EXCLUDED,                                \
        NRFX_UARTE_DEFAULT_EXTENDED_STOP_CONFIG                                     \
        NRFX_UARTE_DEFAULT_EXTENDED_PARITYTYPE_CONFIG                               \
    }                                                                               \
}

/** @brief Structure for the UARTE transfer completion event. */
typedef struct
{
    uint8_t * p_data; ///< Pointer to memory used for transfer.
    size_t    bytes;  ///< Number of bytes transfered.
} nrfx_uarte_xfer_evt_t;

/** @brief Structure for UARTE error event. */
typedef struct
{
    nrfx_uarte_xfer_evt_t rxtx;       ///< Transfer details, including number of bytes transferred.
    uint32_t              error_mask; ///< Mask of error flags that generated the event.
} nrfx_uarte_error_evt_t;

/** @brief Structure for UARTE event. */
typedef struct
{
    nrfx_uarte_evt_type_t type; ///< Event type.
    union
    {
        nrfx_uarte_xfer_evt_t  rxtx;  ///< Data provided for transfer completion events.
        nrfx_uarte_error_evt_t error; ///< Data provided for error event.
    } data;                           ///< Union to store event data.
} nrfx_uarte_event_t;

/**
 * @brief UARTE interrupt event handler.
 *
 * @param[in] p_event   Pointer to event structure. Event is allocated on the stack so it is available
 *                      only within the context of the event handler.
 * @param[in] p_context Context passed to the interrupt handler, set on initialization.
 */
typedef void (*nrfx_uarte_event_handler_t)(nrfx_uarte_event_t const * p_event,
                                           void *                     p_context);

/**
 * @brief Function for initializing the UARTE driver.
 *
 * This function configures and enables UARTE. After this function GPIO pins are controlled by UARTE.
 *
 * @param[in] p_instance    Pointer to the driver instance structure.
 * @param[in] p_config      Pointer to the structure with the initial configuration.
 * @param[in] event_handler Event handler provided by the user. If not provided driver works in
 *                          blocking mode.
 *
 * @retval NRFX_SUCCESS             Initialization was successful.
 * @retval NRFX_ERROR_INVALID_STATE Driver is already initialized.
 * @retval NRFX_ERROR_BUSY          Some other peripheral with the same
 *                                  instance ID is already in use. This is
 *                                  possible only if @ref nrfx_prs module
 *                                  is enabled.
 */
nrfx_err_t nrfx_uarte_init(nrfx_uarte_t const *        p_instance,
                           nrfx_uarte_config_t const * p_config,
                           nrfx_uarte_event_handler_t  event_handler);

/**
 * @brief Function for uninitializing the UARTE driver.
 *
 * @param[in] p_instance Pointer to the driver instance structure.
 */
void nrfx_uarte_uninit(nrfx_uarte_t const * p_instance);

/**
 * @brief Function for getting the address of the specified UARTE task.
 *
 * @param[in] p_instance Pointer to the driver instance structure.
 * @param[in] task       Task.
 *
 * @return Task address.
 */
NRFX_STATIC_INLINE uint32_t nrfx_uarte_task_address_get(nrfx_uarte_t const * p_instance,
                                                        nrf_uarte_task_t     task);

/**
 * @brief Function for getting the address of the specified UARTE event.
 *
 * @param[in] p_instance Pointer to the driver instance structure.
 * @param[in] event      Event.
 *
 * @return Event address.
 */
NRFX_STATIC_INLINE uint32_t nrfx_uarte_event_address_get(nrfx_uarte_t const * p_instance,
                                                         nrf_uarte_event_t    event);

/**
 * @brief Function for sending data over UARTE.
 *
 * If an event handler is provided in nrfx_uarte_init() call, this function
 * returns immediately and the handler is called when the transfer is done.
 * Otherwise, the transfer is performed in blocking mode, that is this function
 * returns when the transfer is finished. Blocking mode is not using interrupt
 * so there is no context switching inside the function.
 *
 * @note Peripherals using EasyDMA (including UARTE) require the transfer buffers
 *       to be placed in the Data RAM region. If this condition is not met,
 *       this function will fail with the error code NRFX_ERROR_INVALID_ADDR.
 *
 * @param[in] p_instance Pointer to the driver instance structure.
 * @param[in] p_data     Pointer to data.
 * @param[in] length     Number of bytes to send. Maximum possible length is
 *                       dependent on the used SoC (see the MAXCNT register
 *                       description in the Product Specification). The driver
 *                       checks it with assertion.
 *
 * @retval NRFX_SUCCESS            Initialization was successful.
 * @retval NRFX_ERROR_BUSY         Driver is already transferring.
 * @retval NRFX_ERROR_FORBIDDEN    The transfer was aborted from a different context
 *                                 (blocking mode only).
 * @retval NRFX_ERROR_INVALID_ADDR p_data does not point to RAM buffer.
 */
nrfx_err_t nrfx_uarte_tx(nrfx_uarte_t const * p_instance,
                         uint8_t const *      p_data,
                         size_t               length);

/**
 * @brief Function for checking if UARTE is currently transmitting.
 *
 * @param[in] p_instance Pointer to the driver instance structure.
 *
 * @retval true  The UARTE is transmitting.
 * @retval false The UARTE is not transmitting.
 */
bool nrfx_uarte_tx_in_progress(nrfx_uarte_t const * p_instance);

/**
 * @brief Function for aborting any ongoing transmission.
 * @note @ref NRFX_UARTE_EVT_TX_DONE event will be generated in non-blocking mode.
 *       It will contain number of bytes sent until the abort was called. The event
 *       handler will be called from the UARTE interrupt context.
 *
 * @param[in] p_instance Pointer to the driver instance structure.
 */
void nrfx_uarte_tx_abort(nrfx_uarte_t const * p_instance);

/**
 * @brief Function for receiving data over UARTE.
 *
 * If an event handler is provided in the nrfx_uarte_init() call, this function
 * returns immediately and the handler is called when the transfer is done.
 * Otherwise, the transfer is performed in blocking mode, that is this function
 * returns when the transfer is finished. Blocking mode is not using interrupt so
 * there is no context switching inside the function.
 * The receive buffer pointer is double-buffered in non-blocking mode. The secondary
 * buffer can be set immediately after starting the transfer and will be filled
 * when the primary buffer is full. The double-buffering feature allows
 * receiving data continuously.
 *
 * @note Peripherals using EasyDMA (including UARTE) require the transfer buffers
 *       to be placed in the Data RAM region. If this condition is not met,
 *       this function fails with the error code NRFX_ERROR_INVALID_ADDR.
 *
 * @warning When the double-buffering feature is used and the UARTE interrupt
 *          is processed with a delay (for example, due to a higher priority interrupt)
 *          long enough for both buffers to get filled completely,
 *          the event handler will be invoked only once, to notify that
 *          the first buffer has been filled. This is because from hardware perspective it
 *          is impossible to deduce in such case if the second buffer was also filled completely or not.
 *          To prevent this from happening, keep the UARTE interrupt latency low
 *          or use large enough reception buffers.
 *
 * @param[in] p_instance Pointer to the driver instance structure.
 * @param[in] p_data     Pointer to data.
 * @param[in] length     Number of bytes to receive. Maximum possible length is
 *                       dependent on the used SoC (see the MAXCNT register
 *                       description in the Product Specification). The driver
 *                       checks it with assertion.
 *
 * @retval NRFX_SUCCESS            Initialization is successful.
 * @retval NRFX_ERROR_BUSY         The driver is already receiving
 *                                 (and the secondary buffer has already been set
 *                                 in non-blocking mode).
 * @retval NRFX_ERROR_FORBIDDEN    The transfer is aborted from a different context
 *                                 (blocking mode only).
 * @retval NRFX_ERROR_INTERNAL     The UARTE peripheral reports an error.
 * @retval NRFX_ERROR_INVALID_ADDR p_data does not point to RAM buffer.
 */
nrfx_err_t nrfx_uarte_rx(nrfx_uarte_t const * p_instance,
                         uint8_t *            p_data,
                         size_t               length);



/**
 * @brief Function for testing the receiver state in blocking mode.
 *
 * @param[in] p_instance Pointer to the driver instance structure.
 *
 * @retval true  The receiver has at least one byte of data to get.
 * @retval false The receiver is empty.
 */
bool nrfx_uarte_rx_ready(nrfx_uarte_t const * p_instance);

/**
 * @brief Function for aborting any ongoing reception.
 * @note @ref NRFX_UARTE_EVT_RX_DONE event will be generated in non-blocking mode.
 *       It will contain number of bytes received until the abort was called. The event
 *       handler will be called from the UARTE interrupt context.
 *
 * @warning When the double-buffering feature is used and the UARTE interrupt
 *          is processed with a delay (for example, due to a higher priority
 *          interrupt) long enough for the first buffer to be filled completely,
 *          the event handler will be supplied with the pointer to the first
 *          buffer and the number of bytes received in the second buffer.
 *          This is because from hardware perspective it is impossible to deduce
 *          the reception of which buffer has been aborted.
 *          To prevent this from happening, keep the UARTE interrupt latency low
 *          or use large enough reception buffers.
 *
 * @param[in] p_instance Pointer to the driver instance structure.
 */
void nrfx_uarte_rx_abort(nrfx_uarte_t const * p_instance);

/**
 * @brief Function for reading error source mask. Mask contains values from @ref nrf_uarte_error_mask_t.
 * @note Function must be used in the blocking mode only. In case of non-blocking mode, an error event is
 *       generated. Function clears error sources after reading.
 *
 * @param[in] p_instance Pointer to the driver instance structure.
 *
 * @return Mask of reported errors.
 */
uint32_t nrfx_uarte_errorsrc_get(nrfx_uarte_t const * p_instance);


#ifndef NRFX_DECLARE_ONLY
NRFX_STATIC_INLINE uint32_t nrfx_uarte_task_address_get(nrfx_uarte_t const * p_instance,
                                                        nrf_uarte_task_t     task)
{
    return nrf_uarte_task_address_get(p_instance->p_reg, task);
}

NRFX_STATIC_INLINE uint32_t nrfx_uarte_event_address_get(nrfx_uarte_t const * p_instance,
                                                         nrf_uarte_event_t    event)
{
    return nrf_uarte_event_address_get(p_instance->p_reg, event);
}
#endif // NRFX_DECLARE_ONLY

/** @} */


void nrfx_uarte_0_irq_handler(void);
void nrfx_uarte_1_irq_handler(void);
void nrfx_uarte_2_irq_handler(void);
void nrfx_uarte_3_irq_handler(void);


#ifdef __cplusplus
}
#endif

#endif // NRFX_UARTE_H__
