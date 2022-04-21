/*
 * Copyright (c) 2014 - 2022, Nordic Semiconductor ASA
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

#ifndef NRF_LPCOMP_H_
#define NRF_LPCOMP_H_

#include <nrfx.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup nrf_lpcomp_hal LPCOMP HAL
 * @{
 * @ingroup nrf_lpcomp
 * @brief   Hardware access layer for managing the Low Power Comparator (LPCOMP) peripheral.
 */

/** @brief LPCOMP reference selection. */
typedef enum
{
#if (LPCOMP_REFSEL_RESOLUTION == 8) || defined(__NRFX_DOXYGEN__)
    NRF_LPCOMP_REF_SUPPLY_1_8   = LPCOMP_REFSEL_REFSEL_SupplyOneEighthPrescaling,    /**< Use supply with a 1/8 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_2_8   = LPCOMP_REFSEL_REFSEL_SupplyTwoEighthsPrescaling,   /**< Use supply with a 2/8 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_3_8   = LPCOMP_REFSEL_REFSEL_SupplyThreeEighthsPrescaling, /**< Use supply with a 3/8 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_4_8   = LPCOMP_REFSEL_REFSEL_SupplyFourEighthsPrescaling,  /**< Use supply with a 4/8 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_5_8   = LPCOMP_REFSEL_REFSEL_SupplyFiveEighthsPrescaling,  /**< Use supply with a 5/8 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_6_8   = LPCOMP_REFSEL_REFSEL_SupplySixEighthsPrescaling,   /**< Use supply with a 6/8 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_7_8   = LPCOMP_REFSEL_REFSEL_SupplySevenEighthsPrescaling, /**< Use supply with a 7/8 prescaler as reference. */
#elif (LPCOMP_REFSEL_RESOLUTION == 16) || defined(__NRFX_DOXYGEN__)
    NRF_LPCOMP_REF_SUPPLY_1_8   = LPCOMP_REFSEL_REFSEL_Ref1_8Vdd,   /**< Use supply with a 1/8 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_2_8   = LPCOMP_REFSEL_REFSEL_Ref2_8Vdd,   /**< Use supply with a 2/8 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_3_8   = LPCOMP_REFSEL_REFSEL_Ref3_8Vdd,   /**< Use supply with a 3/8 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_4_8   = LPCOMP_REFSEL_REFSEL_Ref4_8Vdd,   /**< Use supply with a 4/8 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_5_8   = LPCOMP_REFSEL_REFSEL_Ref5_8Vdd,   /**< Use supply with a 5/8 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_6_8   = LPCOMP_REFSEL_REFSEL_Ref6_8Vdd,   /**< Use supply with a 6/8 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_7_8   = LPCOMP_REFSEL_REFSEL_Ref7_8Vdd,   /**< Use supply with a 7/8 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_1_16  = LPCOMP_REFSEL_REFSEL_Ref1_16Vdd,  /**< Use supply with a 1/16 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_3_16  = LPCOMP_REFSEL_REFSEL_Ref3_16Vdd,  /**< Use supply with a 3/16 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_5_16  = LPCOMP_REFSEL_REFSEL_Ref5_16Vdd,  /**< Use supply with a 5/16 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_7_16  = LPCOMP_REFSEL_REFSEL_Ref7_16Vdd,  /**< Use supply with a 7/16 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_9_16  = LPCOMP_REFSEL_REFSEL_Ref9_16Vdd,  /**< Use supply with a 9/16 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_11_16 = LPCOMP_REFSEL_REFSEL_Ref11_16Vdd, /**< Use supply with a 11/16 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_13_16 = LPCOMP_REFSEL_REFSEL_Ref13_16Vdd, /**< Use supply with a 13/16 prescaler as reference. */
    NRF_LPCOMP_REF_SUPPLY_15_16 = LPCOMP_REFSEL_REFSEL_Ref15_16Vdd, /**< Use supply with a 15/16 prescaler as reference. */
#endif
    NRF_LPCOMP_REF_EXT_REF0     = LPCOMP_REFSEL_REFSEL_ARef |
                                  (LPCOMP_EXTREFSEL_EXTREFSEL_AnalogReference0 << 16), /**< External reference 0. */
    NRF_LPCOMP_REF_EXT_REF1     = LPCOMP_REFSEL_REFSEL_ARef |
                                  (LPCOMP_EXTREFSEL_EXTREFSEL_AnalogReference1 << 16), /**< External reference 1. */
} nrf_lpcomp_ref_t;

/** @brief LPCOMP input selection. */
typedef enum
{
    NRF_LPCOMP_INPUT_0 = LPCOMP_PSEL_PSEL_AnalogInput0, /**< Input 0. */
    NRF_LPCOMP_INPUT_1 = LPCOMP_PSEL_PSEL_AnalogInput1, /**< Input 1. */
    NRF_LPCOMP_INPUT_2 = LPCOMP_PSEL_PSEL_AnalogInput2, /**< Input 2. */
    NRF_LPCOMP_INPUT_3 = LPCOMP_PSEL_PSEL_AnalogInput3, /**< Input 3. */
    NRF_LPCOMP_INPUT_4 = LPCOMP_PSEL_PSEL_AnalogInput4, /**< Input 4. */
    NRF_LPCOMP_INPUT_5 = LPCOMP_PSEL_PSEL_AnalogInput5, /**< Input 5. */
    NRF_LPCOMP_INPUT_6 = LPCOMP_PSEL_PSEL_AnalogInput6, /**< Input 6. */
    NRF_LPCOMP_INPUT_7 = LPCOMP_PSEL_PSEL_AnalogInput7  /**< Input 7. */
} nrf_lpcomp_input_t;

/** @brief LPCOMP detection type selection. */
typedef enum
{
    NRF_LPCOMP_DETECT_CROSS = LPCOMP_ANADETECT_ANADETECT_Cross, /**< Generate ANADETEC on crossing, both upwards and downwards crossing. */
    NRF_LPCOMP_DETECT_UP    = LPCOMP_ANADETECT_ANADETECT_Up,    /**< Generate ANADETEC on upwards crossing only. */
    NRF_LPCOMP_DETECT_DOWN  = LPCOMP_ANADETECT_ANADETECT_Down   /**< Generate ANADETEC on downwards crossing only. */
} nrf_lpcomp_detect_t;

/** @brief LPCOMP tasks. */
typedef enum
{
    NRF_LPCOMP_TASK_START  = offsetof(NRF_LPCOMP_Type, TASKS_START), /**< LPCOMP start sampling task. */
    NRF_LPCOMP_TASK_STOP   = offsetof(NRF_LPCOMP_Type, TASKS_STOP),  /**< LPCOMP stop sampling task. */
    NRF_LPCOMP_TASK_SAMPLE = offsetof(NRF_LPCOMP_Type, TASKS_SAMPLE) /**< Sample comparator value. */
} nrf_lpcomp_task_t;


/** @brief LPCOMP events. */
typedef enum
{
    NRF_LPCOMP_EVENT_READY = offsetof(NRF_LPCOMP_Type, EVENTS_READY), /**< LPCOMP is ready and output is valid. */
    NRF_LPCOMP_EVENT_DOWN  = offsetof(NRF_LPCOMP_Type, EVENTS_DOWN),  /**< Input voltage crossed the threshold going down. */
    NRF_LPCOMP_EVENT_UP    = offsetof(NRF_LPCOMP_Type, EVENTS_UP),    /**< Input voltage crossed the threshold going up. */
    NRF_LPCOMP_EVENT_CROSS = offsetof(NRF_LPCOMP_Type, EVENTS_CROSS)  /**< Input voltage crossed the threshold in any direction. */
} nrf_lpcomp_event_t;

/** @brief LPCOMP interrupts. */
typedef enum
{
    NRF_LPCOMP_INT_READY_MASK = LPCOMP_INTENSET_READY_Msk, /**< Interrupt on READY event. */
    NRF_LPCOMP_INT_DOWN_MASK  = LPCOMP_INTENSET_DOWN_Msk,  /**< Interrupt on DOWN event. */
    NRF_LPCOMP_INT_UP_MASK    = LPCOMP_INTENSET_UP_Msk,    /**< Interrupt on UP event. */
    NRF_LPCOMP_INT_CROSS_MASK = LPCOMP_INTENSET_CROSS_Msk  /**< Interrupt on CROSS event. */
} nrf_lpcomp_int_mask_t;

/** @brief LPCOMP shortcut masks. */
typedef enum
{
    NRF_LPCOMP_SHORT_CROSS_STOP_MASK   = LPCOMP_SHORTS_CROSS_STOP_Msk,  /*!< Shortcut between CROSS event and STOP task. */
    NRF_LPCOMP_SHORT_UP_STOP_MASK      = LPCOMP_SHORTS_UP_STOP_Msk,     /*!< Shortcut between UP event and STOP task. */
    NRF_LPCOMP_SHORT_DOWN_STOP_MASK    = LPCOMP_SHORTS_DOWN_STOP_Msk,   /*!< Shortcut between DOWN event and STOP task. */
    NRF_LPCOMP_SHORT_READY_STOP_MASK   = LPCOMP_SHORTS_READY_STOP_Msk,  /*!< Shortcut between READY event and STOP task. */
    NRF_LPCOMP_SHORT_READY_SAMPLE_MASK = LPCOMP_SHORTS_READY_SAMPLE_Msk /*!< Shortcut between READY event and SAMPLE task. */
} nrf_lpcomp_short_mask_t;

#if defined(LPCOMP_FEATURE_HYST_PRESENT) || defined(__NRFX_DOXYGEN__)
/** @brief LPCOMP hysteresis. */
typedef enum
{
#ifdef LPCOMP_HYST_HYST_NoHyst
    NRF_LPCOMP_HYST_NOHYST  = LPCOMP_HYST_HYST_NoHyst,   /**< Comparator hysteresis disabled. */
#else
    NRF_LPCOMP_HYST_NOHYST  = LPCOMP_HYST_HYST_Disabled, /**< Comparator hysteresis disabled. */
#endif
#ifdef LPCOMP_HYST_HYST_Hyst50mV
    NRF_LPCOMP_HYST_ENABLED = LPCOMP_HYST_HYST_Hyst50mV  /**< Comparator hysteresis enabled (typically 50 mV). */
#else
    NRF_LPCOMP_HYST_ENABLED = LPCOMP_HYST_HYST_Enabled   /**< Comparator hysteresis enabled (typically 50 mV). */
#endif
}nrf_lpcomp_hysteresis_t;
#endif // LPCOMP_FEATURE_HYST_PRESENT

/** @brief LPCOMP configuration. */
typedef struct
{
    nrf_lpcomp_ref_t        reference; /**< LPCOMP reference. */
    nrf_lpcomp_detect_t     detection; /**< LPCOMP detection type. */
#if defined(LPCOMP_FEATURE_HYST_PRESENT) || defined(__NRFX_DOXYGEN__)
    nrf_lpcomp_hysteresis_t hyst;      /**< LPCOMP hysteresis. */
#endif // LPCOMP_FEATURE_HYST_PRESENT
} nrf_lpcomp_config_t;

/**
 * @brief Function for configuring LPCOMP.
 *
 * This function powers on LPCOMP and configures it. LPCOMP is in DISABLE state after configuration,
 * so it must be enabled before using it. All shorts are inactive, events are cleared, and LPCOMP is stopped.
 *
 * @param[in] p_reg    Pointer to the structure of registers of the peripheral.
 * @param[in] p_config Configuration.
 */
NRF_STATIC_INLINE void nrf_lpcomp_configure(NRF_LPCOMP_Type *           p_reg,
                                            nrf_lpcomp_config_t const * p_config);

/**
 * @brief Function for selecting the LPCOMP input.
 *
 * This function selects the active input of LPCOMP.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] input Input to be selected.
 */
NRF_STATIC_INLINE void nrf_lpcomp_input_select(NRF_LPCOMP_Type * p_reg, nrf_lpcomp_input_t input);

/**
 * @brief Function for enabling the Low Power Comparator.
 *
 * This function enables LPCOMP.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 */
NRF_STATIC_INLINE void nrf_lpcomp_enable(NRF_LPCOMP_Type * p_reg);

/**
 * @brief Function for disabling the Low Power Comparator.
 *
 * This function disables LPCOMP.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 */
NRF_STATIC_INLINE void nrf_lpcomp_disable(NRF_LPCOMP_Type * p_reg);

/**
 * @brief Function for getting the last LPCOMP compare result.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 *
 * @return The last compare result. If 0 then VIN+ < VIN-, if 1 then the opposite.
 */
NRF_STATIC_INLINE uint32_t nrf_lpcomp_result_get(NRF_LPCOMP_Type const * p_reg);

/**
 * @brief Function for enabling interrupts from LPCOMP.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] mask  Mask of interrupts to be enabled.
 *
 * @sa nrf_lpcomp_int_disable
 * @sa nrf_lpcomp_int_enable_check
 */
NRF_STATIC_INLINE void nrf_lpcomp_int_enable(NRF_LPCOMP_Type * p_reg, uint32_t mask);

/**
 * @brief Function for disabling interrupts from LPCOMP.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] mask  Mask of interrupts to be disabled.
 *
 * @sa nrf_lpcomp_int_enable
 * @sa nrf_lpcomp_int_enable_check
 */
NRF_STATIC_INLINE void nrf_lpcomp_int_disable(NRF_LPCOMP_Type * p_reg, uint32_t mask);

/**
 * @brief Function for checking if the specified interrupts are enabled.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] mask  Mask of interrupts to be checked.
 *
 * @return Mask of enabled interrupts.
 */
NRF_STATIC_INLINE uint32_t nrf_lpcomp_int_enable_check(NRF_LPCOMP_Type const * p_reg,
                                                       uint32_t                mask);

/**
 * @brief Function for getting the address of the specified LPCOMP task register.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] task  LPCOMP task.
 *
 * @return The address of the specified LPCOMP task.
 */
NRF_STATIC_INLINE uint32_t nrf_lpcomp_task_address_get(NRF_LPCOMP_Type const * p_reg,
                                                       nrf_lpcomp_task_t       task);

/**
 * @brief Function for getting the address of the specified LPCOMP event register.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] event LPCOMP event.
 *
 * @return The address of the specified LPCOMP event.
 */
NRF_STATIC_INLINE uint32_t nrf_lpcomp_event_address_get(NRF_LPCOMP_Type const * p_reg,
                                                        nrf_lpcomp_event_t      event);

/**
 * @brief  Function for setting LPCOMP shorts.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] mask  Mask of shortcuts.
 */
NRF_STATIC_INLINE void nrf_lpcomp_shorts_enable(NRF_LPCOMP_Type * p_reg, uint32_t mask);

/**
 * @brief Function for clearing LPCOMP shorts by mask.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] mask  Mask of shortcuts.
 */
NRF_STATIC_INLINE void nrf_lpcomp_shorts_disable(NRF_LPCOMP_Type * p_reg, uint32_t mask);

/**
 * @brief Function for setting the specified LPCOMP task.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] task  LPCOMP task to be set.
 */
NRF_STATIC_INLINE void nrf_lpcomp_task_trigger(NRF_LPCOMP_Type * p_reg, nrf_lpcomp_task_t task);

/**
 * @brief Function for clearing the specified LPCOMP event.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] event LPCOMP event to be cleared.
 */
NRF_STATIC_INLINE void nrf_lpcomp_event_clear(NRF_LPCOMP_Type * p_reg, nrf_lpcomp_event_t event);

/**
 * @brief Function for retrieving the state of the LPCOMP event.
 *
 * @param[in] p_reg Pointer to the structure of registers of the peripheral.
 * @param[in] event Event to be checked.
 *
 * @retval true  The event has been generated.
 * @retval false The event has not been generated.
 */
NRF_STATIC_INLINE bool nrf_lpcomp_event_check(NRF_LPCOMP_Type const * p_reg,
                                              nrf_lpcomp_event_t      event);

#ifndef NRF_DECLARE_ONLY

NRF_STATIC_INLINE void nrf_lpcomp_configure(NRF_LPCOMP_Type *           p_reg,
                                            nrf_lpcomp_config_t const * p_config)
{
    p_reg->TASKS_STOP = 1;
    p_reg->ENABLE     = LPCOMP_ENABLE_ENABLE_Disabled << LPCOMP_ENABLE_ENABLE_Pos;
    p_reg->REFSEL     =
        (p_config->reference << LPCOMP_REFSEL_REFSEL_Pos) & LPCOMP_REFSEL_REFSEL_Msk;

    //If external source is choosen extract analog reference index.
    if ((p_config->reference & LPCOMP_REFSEL_REFSEL_ARef)==LPCOMP_REFSEL_REFSEL_ARef)
    {
        uint32_t extref  = p_config->reference >> 16;
        p_reg->EXTREFSEL = (extref << LPCOMP_EXTREFSEL_EXTREFSEL_Pos) &
                           LPCOMP_EXTREFSEL_EXTREFSEL_Msk;
    }

    p_reg->ANADETECT = (p_config->detection << LPCOMP_ANADETECT_ANADETECT_Pos) &
                       LPCOMP_ANADETECT_ANADETECT_Msk;
#ifdef LPCOMP_FEATURE_HYST_PRESENT
    p_reg->HYST      = ((p_config->hyst) << LPCOMP_HYST_HYST_Pos) & LPCOMP_HYST_HYST_Msk;
#endif //LPCOMP_FEATURE_HYST_PRESENT
    p_reg->SHORTS    = 0;
    p_reg->INTENCLR  = LPCOMP_INTENCLR_CROSS_Msk | LPCOMP_INTENCLR_UP_Msk |
                       LPCOMP_INTENCLR_DOWN_Msk | LPCOMP_INTENCLR_READY_Msk;
}

NRF_STATIC_INLINE void nrf_lpcomp_input_select(NRF_LPCOMP_Type * p_reg, nrf_lpcomp_input_t input)
{
    uint32_t lpcomp_enable_state = p_reg->ENABLE;

    p_reg->ENABLE = LPCOMP_ENABLE_ENABLE_Disabled << LPCOMP_ENABLE_ENABLE_Pos;
    p_reg->PSEL   = ((uint32_t)input << LPCOMP_PSEL_PSEL_Pos) |
                    (p_reg->PSEL & ~LPCOMP_PSEL_PSEL_Msk);
    p_reg->ENABLE = lpcomp_enable_state;
}

NRF_STATIC_INLINE void nrf_lpcomp_enable(NRF_LPCOMP_Type * p_reg)
{
    p_reg->ENABLE = LPCOMP_ENABLE_ENABLE_Enabled << LPCOMP_ENABLE_ENABLE_Pos;
    p_reg->EVENTS_READY = 0;
    p_reg->EVENTS_DOWN  = 0;
    p_reg->EVENTS_UP    = 0;
    p_reg->EVENTS_CROSS = 0;
}

NRF_STATIC_INLINE void nrf_lpcomp_disable(NRF_LPCOMP_Type * p_reg)
{
    p_reg->ENABLE = LPCOMP_ENABLE_ENABLE_Disabled << LPCOMP_ENABLE_ENABLE_Pos;
}

NRF_STATIC_INLINE uint32_t nrf_lpcomp_result_get(NRF_LPCOMP_Type const * p_reg)
{
    return (uint32_t)p_reg->RESULT;
}

NRF_STATIC_INLINE void nrf_lpcomp_int_enable(NRF_LPCOMP_Type * p_reg, uint32_t mask)
{
    p_reg->INTENSET = mask;
}

NRF_STATIC_INLINE void nrf_lpcomp_int_disable(NRF_LPCOMP_Type * p_reg, uint32_t mask)
{
    p_reg->INTENCLR = mask;
}

NRF_STATIC_INLINE uint32_t nrf_lpcomp_int_enable_check(NRF_LPCOMP_Type const * p_reg,
                                                       uint32_t                mask)
{
    return p_reg->INTENSET & mask; // when read this register will return the value of INTEN.
}

NRF_STATIC_INLINE uint32_t nrf_lpcomp_task_address_get(NRF_LPCOMP_Type const * p_reg,
                                                       nrf_lpcomp_task_t       task)
{
    return (uint32_t)((uint8_t *)p_reg + task);
}

NRF_STATIC_INLINE uint32_t nrf_lpcomp_event_address_get(NRF_LPCOMP_Type const * p_reg,
                                                        nrf_lpcomp_event_t      event)
{
    return (uint32_t)((uint8_t *)p_reg + event);
}

NRF_STATIC_INLINE void nrf_lpcomp_shorts_enable(NRF_LPCOMP_Type * p_reg, uint32_t short_mask)
{
    p_reg->SHORTS |= short_mask;
}

NRF_STATIC_INLINE void nrf_lpcomp_shorts_disable(NRF_LPCOMP_Type * p_reg, uint32_t short_mask)
{
    p_reg->SHORTS &= ~short_mask;
}

NRF_STATIC_INLINE void nrf_lpcomp_task_trigger(NRF_LPCOMP_Type * p_reg, nrf_lpcomp_task_t task)
{
    *( (volatile uint32_t *)( (uint8_t *)p_reg + (uint32_t)task) ) = 1;
}

NRF_STATIC_INLINE void nrf_lpcomp_event_clear(NRF_LPCOMP_Type * p_reg, nrf_lpcomp_event_t event)
{
    *( (volatile uint32_t *)( (uint8_t *)p_reg + (uint32_t)event) ) = 0;
    nrf_event_readback((uint8_t *)p_reg + (uint32_t)event);
}

NRF_STATIC_INLINE bool nrf_lpcomp_event_check(NRF_LPCOMP_Type const * p_reg,
                                              nrf_lpcomp_event_t      event)
{
    return (bool) (*(volatile uint32_t *)( (uint8_t *)p_reg + (uint32_t)event));
}

#endif // NRF_DECLARE_ONLY

/** @} */

#ifdef __cplusplus
}
#endif

#endif // NRF_LPCOMP_H_
