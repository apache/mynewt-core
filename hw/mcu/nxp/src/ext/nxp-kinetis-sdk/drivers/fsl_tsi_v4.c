/*
 * Copyright (c) 2014 - 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2019 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "fsl_tsi_v4.h"

/* Component ID definition, used by tools. */
#ifndef FSL_COMPONENT_ID
#define FSL_COMPONENT_ID "platform.drivers.tsi_v4"
#endif

/*!
 * brief Initializes hardware.
 *
 * details Initializes the peripheral to the targeted state specified by parameter configuration,
 *          such as sets prescalers, number of scans, clocks, delta voltage
 *          series resistor, filter bits, reference, and electrode charge current and threshold.
 * param  base    TSI peripheral base address.
 * param  config  Pointer to TSI module configuration structure.
 * return none
 */
void TSI_Init(TSI_Type *base, const tsi_config_t *config)
{
    assert(config != NULL);

    bool is_module_enabled = false;
    bool is_int_enabled    = false;

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    CLOCK_EnableClock(kCLOCK_Tsi0);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
    if ((bool)(base->GENCS & TSI_GENCS_TSIEN_MASK))
    {
        is_module_enabled = true;
        TSI_EnableModule(base, false);
    }
    if ((bool)(base->GENCS & TSI_GENCS_TSIIEN_MASK))
    {
        is_int_enabled = true;
        TSI_DisableInterrupts(base, (uint32_t)kTSI_GlobalInterruptEnable);
    }

    if (config->mode == kTSI_AnalogModeSel_Capacitive)
    {
        TSI_SetHighThreshold(base, config->thresh);
        TSI_SetLowThreshold(base, config->thresl);
        TSI_SetElectrodeOSCPrescaler(base, config->prescaler);
        TSI_SetReferenceChargeCurrent(base, config->refchrg);
        TSI_SetElectrodeChargeCurrent(base, config->extchrg);
        TSI_SetNumberOfScans(base, config->nscn);
        TSI_SetAnalogMode(base, config->mode);
        TSI_SetOscVoltageRails(base, config->dvolt);
    }
    else /* For noise modes */
    {
        TSI_SetHighThreshold(base, config->thresh);
        TSI_SetLowThreshold(base, config->thresl);
        TSI_SetElectrodeOSCPrescaler(base, config->prescaler);
        TSI_SetReferenceChargeCurrent(base, config->refchrg);
        TSI_SetNumberOfScans(base, config->nscn);
        TSI_SetAnalogMode(base, config->mode);
        TSI_SetOscVoltageRails(base, config->dvolt);
        TSI_SetElectrodeSeriesResistor(base, config->resistor);
        TSI_SetFilterBits(base, config->filter);
    }

    if (is_module_enabled)
    {
        TSI_EnableModule(base, true);
    }
    if (is_int_enabled)
    {
        TSI_EnableInterrupts(base, (uint32_t)kTSI_GlobalInterruptEnable);
    }
}

/*!
 * brief De-initializes hardware.
 *
 * details De-initializes the peripheral to default state.
 *
 * param  base  TSI peripheral base address.
 * return none
 */
void TSI_Deinit(TSI_Type *base)
{
    base->GENCS = 0U;
    base->DATA  = 0U;
    base->TSHD  = 0U;
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    CLOCK_DisableClock(kCLOCK_Tsi0);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
}

/*!
 * brief Gets the TSI normal mode user configuration structure.
 * This interface sets userConfig structure to a default value. The configuration structure only
 * includes the settings for the whole TSI.
 * The user configure is set to these values:
 * code
    userConfig->prescaler = kTSI_ElecOscPrescaler_2div;
    userConfig->extchrg = kTSI_ExtOscChargeCurrent_500nA;
    userConfig->refchrg = kTSI_RefOscChargeCurrent_4uA;
    userConfig->nscn = kTSI_ConsecutiveScansNumber_10time;
    userConfig->mode = kTSI_AnalogModeSel_Capacitive;
    userConfig->dvolt = kTSI_OscVolRailsOption_0;
    userConfig->thresh = 0U;
    userConfig->thresl = 0U;
   endcode
 *
 * param userConfig Pointer to the TSI user configuration structure.
 */
void TSI_GetNormalModeDefaultConfig(tsi_config_t *userConfig)
{
    /* Initializes the configure structure to zero. */
    (void)memset(userConfig, 0, sizeof(*userConfig));

    userConfig->thresh    = 0U;
    userConfig->thresl    = 0U;
    userConfig->prescaler = kTSI_ElecOscPrescaler_2div;
    userConfig->extchrg   = kTSI_ExtOscChargeCurrent_500nA;
    userConfig->refchrg   = kTSI_RefOscChargeCurrent_4uA;
    userConfig->nscn      = kTSI_ConsecutiveScansNumber_5time;
    userConfig->mode      = kTSI_AnalogModeSel_Capacitive;
    userConfig->dvolt     = kTSI_OscVolRailsOption_0;
}

/*!
 * brief Gets the TSI low power mode default user configuration structure.
 * This interface sets userConfig structure to a default value. The configuration structure only
 * includes the settings for the whole TSI.
 * The user configure is set to these values:
 * code
    userConfig->prescaler = kTSI_ElecOscPrescaler_2div;
    userConfig->extchrg = kTSI_ExtOscChargeCurrent_500nA;
    userConfig->refchrg = kTSI_RefOscChargeCurrent_4uA;
    userConfig->nscn = kTSI_ConsecutiveScansNumber_10time;
    userConfig->mode = kTSI_AnalogModeSel_Capacitive;
    userConfig->dvolt = kTSI_OscVolRailsOption_0;
    userConfig->thresh = 400U;
    userConfig->thresl = 0U;
   endcode
 *
 * param userConfig Pointer to the TSI user configuration structure.
 */
void TSI_GetLowPowerModeDefaultConfig(tsi_config_t *userConfig)
{
    /* Initializes the configure structure to zero. */
    (void)memset(userConfig, 0, sizeof(*userConfig));

    userConfig->thresh    = 400U;
    userConfig->thresl    = 0U;
    userConfig->prescaler = kTSI_ElecOscPrescaler_2div;
    userConfig->extchrg   = kTSI_ExtOscChargeCurrent_500nA;
    userConfig->refchrg   = kTSI_RefOscChargeCurrent_4uA;
    userConfig->nscn      = kTSI_ConsecutiveScansNumber_5time;
    userConfig->mode      = kTSI_AnalogModeSel_Capacitive;
    userConfig->dvolt     = kTSI_OscVolRailsOption_0;
}

/*!
 * brief Hardware calibration.
 *
 * details Calibrates the peripheral to fetch the initial counter value of
 *          the enabled electrodes.
 *          This API is mostly used at initial application setup. Call
 *          this function after the \ref TSI_Init API and use the calibrated
 *          counter values to set up applications (such as to determine
 *          under which counter value we can confirm a touch event occurs).
 *
 * param   base    TSI peripheral base address.
 * param   calBuff Data buffer that store the calibrated counter value.
 * return none
 *
 */
void TSI_Calibrate(TSI_Type *base, tsi_calibration_data_t *calBuff)
{
    assert(calBuff != NULL);

    uint8_t i           = 0U;
    bool is_int_enabled = false;

    if ((bool)(base->GENCS & TSI_GENCS_TSIIEN_MASK))
    {
        is_int_enabled = true;
        TSI_DisableInterrupts(base, (uint32_t)kTSI_GlobalInterruptEnable);
    }
    for (i = 0U; i < (uint8_t)FSL_FEATURE_TSI_CHANNEL_COUNT; i++)
    {
        TSI_SetMeasuredChannelNumber(base, i);
        TSI_StartSoftwareTrigger(base);
        while (0UL == (TSI_GetStatusFlags(base) & (uint32_t)kTSI_EndOfScanFlag))
        {
        }
        calBuff->calibratedData[i] = TSI_GetCounter(base);
        TSI_ClearStatusFlags(base, (uint32_t)kTSI_EndOfScanFlag);
    }
    if (is_int_enabled)
    {
        TSI_EnableInterrupts(base, (uint32_t)kTSI_GlobalInterruptEnable);
    }
}

/*!
 * brief Enables the TSI interrupt requests.
 * param base TSI peripheral base address.
 * param mask interrupt source
 *     The parameter can be combination of the following source if defined:
 *     arg kTSI_GlobalInterruptEnable
 *     arg kTSI_EndOfScanInterruptEnable
 *     arg kTSI_OutOfRangeInterruptEnable
 */
void TSI_EnableInterrupts(TSI_Type *base, uint32_t mask)
{
    uint32_t regValue = base->GENCS & (~ALL_FLAGS_MASK);

    if (0UL != (mask & (uint32_t)kTSI_GlobalInterruptEnable))
    {
        regValue |= TSI_GENCS_TSIIEN_MASK;
    }
    if (0UL != (mask & (uint32_t)kTSI_OutOfRangeInterruptEnable))
    {
        regValue &= (~TSI_GENCS_ESOR_MASK);
    }
    if (0UL != (mask & (uint32_t)kTSI_EndOfScanInterruptEnable))
    {
        regValue |= TSI_GENCS_ESOR_MASK;
    }

    base->GENCS = regValue; /* write value to register */
}

/*!
 * brief Disables the TSI interrupt requests.
 * param base TSI peripheral base address.
 * param mask interrupt source
 *     The parameter can be combination of the following source if defined:
 *     arg kTSI_GlobalInterruptEnable
 *     arg kTSI_EndOfScanInterruptEnable
 *     arg kTSI_OutOfRangeInterruptEnable
 */
void TSI_DisableInterrupts(TSI_Type *base, uint32_t mask)
{
    uint32_t regValue = base->GENCS & (~ALL_FLAGS_MASK);

    if (0UL != (mask & (uint32_t)kTSI_GlobalInterruptEnable))
    {
        regValue &= (~TSI_GENCS_TSIIEN_MASK);
    }
    if (0UL != (mask & (uint32_t)kTSI_OutOfRangeInterruptEnable))
    {
        regValue |= TSI_GENCS_ESOR_MASK;
    }
    if (0UL != (mask & (uint32_t)kTSI_EndOfScanInterruptEnable))
    {
        regValue &= (~TSI_GENCS_ESOR_MASK);
    }

    base->GENCS = regValue; /* write value to register */
}

/*!
 * brief Clears the interrupt flag.
 *
 * This function clears the TSI interrupt flag,
 * automatically cleared flags can't be cleared by this function.
 *
 * param base TSI peripheral base address.
 * param mask The status flags to clear.
 */
void TSI_ClearStatusFlags(TSI_Type *base, uint32_t mask)
{
    uint32_t regValue = base->GENCS & (~ALL_FLAGS_MASK);

    if (0UL != (mask & (uint32_t)kTSI_EndOfScanFlag))
    {
        regValue |= TSI_GENCS_EOSF_MASK;
    }
    if (0UL != (mask & (uint32_t)kTSI_OutOfRangeFlag))
    {
        regValue |= TSI_GENCS_OUTRGF_MASK;
    }

    base->GENCS = regValue; /* write value to register */
}
