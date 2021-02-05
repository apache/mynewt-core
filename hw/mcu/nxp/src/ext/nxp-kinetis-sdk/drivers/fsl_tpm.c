/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_tpm.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* Component ID definition, used by tools. */
#ifndef FSL_COMPONENT_ID
#define FSL_COMPONENT_ID "platform.drivers.tpm"
#endif

#define TPM_COMBINE_SHIFT (8U)

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/*!
 * @brief Gets the instance from the base address
 *
 * @param base TPM peripheral base address
 *
 * @return The TPM instance
 */
static uint32_t TPM_GetInstance(TPM_Type *base);

/*******************************************************************************
 * Variables
 ******************************************************************************/
/*! @brief Pointers to TPM bases for each instance. */
static TPM_Type *const s_tpmBases[] = TPM_BASE_PTRS;

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
/*! @brief Pointers to TPM clocks for each instance. */
static const clock_ip_name_t s_tpmClocks[] = TPM_CLOCKS;
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */

/*******************************************************************************
 * Code
 ******************************************************************************/
static uint32_t TPM_GetInstance(TPM_Type *base)
{
    uint32_t instance;
    uint32_t tpmArrayCount = (sizeof(s_tpmBases) / sizeof(s_tpmBases[0]));

    /* Find the instance index from base address mappings. */
    for (instance = 0; instance < tpmArrayCount; instance++)
    {
        if (s_tpmBases[instance] == base)
        {
            break;
        }
    }

    assert(instance < tpmArrayCount);

    return instance;
}

/*!
 * brief Ungates the TPM clock and configures the peripheral for basic operation.
 *
 * note This API should be called at the beginning of the application using the TPM driver.
 *
 * param base   TPM peripheral base address
 * param config Pointer to user's TPM config structure.
 */
void TPM_Init(TPM_Type *base, const tpm_config_t *config)
{
    assert(NULL != config);

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    /* Enable the module clock */
    (void)CLOCK_EnableClock(s_tpmClocks[TPM_GetInstance(base)]);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */

#if defined(FSL_FEATURE_TPM_HAS_GLOBAL) && FSL_FEATURE_TPM_HAS_GLOBAL
    /* TPM reset is available on certain SoC's */
    TPM_Reset(base);
#endif

    /* Set the clock prescale factor */
    base->SC = TPM_SC_PS(config->prescale);
#if !(defined(FSL_FEATURE_TPM_HAS_NO_CONF) && FSL_FEATURE_TPM_HAS_NO_CONF)
    /* Setup the counter operation */
    base->CONF = TPM_CONF_DOZEEN(config->enableDoze) | TPM_CONF_GTBEEN(config->useGlobalTimeBase) |
                 TPM_CONF_CROT(config->enableReloadOnTrigger) | TPM_CONF_CSOT(config->enableStartOnTrigger) |
                 TPM_CONF_CSOO(config->enableStopOnOverflow) |
#if defined(FSL_FEATURE_TPM_HAS_PAUSE_COUNTER_ON_TRIGGER) && FSL_FEATURE_TPM_HAS_PAUSE_COUNTER_ON_TRIGGER
                 TPM_CONF_CPOT(config->enablePauseOnTrigger) |
#endif
#if defined(FSL_FEATURE_TPM_HAS_EXTERNAL_TRIGGER_SELECTION) && FSL_FEATURE_TPM_HAS_EXTERNAL_TRIGGER_SELECTION
                 TPM_CONF_TRGSRC(config->triggerSource) |
#endif
                 TPM_CONF_TRGSEL(config->triggerSelect);
    if (true == config->enableDebugMode)
    {
        base->CONF |= TPM_CONF_DBGMODE_MASK;
    }
    else
    {
        base->CONF &= ~TPM_CONF_DBGMODE_MASK;
    }
#endif
}

/*!
 * brief Stops the counter and gates the TPM clock
 *
 * param base TPM peripheral base address
 */
void TPM_Deinit(TPM_Type *base)
{
#if defined(FSL_FEATURE_TPM_HAS_SC_CLKS) && FSL_FEATURE_TPM_HAS_SC_CLKS
    /* Stop the counter */
    base->SC &= ~TPM_SC_CLKS_MASK;
#else
    /* Stop the counter */
    base->SC &= ~TPM_SC_CMOD_MASK;
#endif
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    /* Gate the TPM clock */
    (void)CLOCK_DisableClock(s_tpmClocks[TPM_GetInstance(base)]);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
}

/*!
 * brief  Fill in the TPM config struct with the default settings
 *
 * The default values are:
 * code
 *     config->prescale = kTPM_Prescale_Divide_1;
 *     config->useGlobalTimeBase = false;
 *     config->dozeEnable = false;
 *     config->dbgMode = false;
 *     config->enableReloadOnTrigger = false;
 *     config->enableStopOnOverflow = false;
 *     config->enableStartOnTrigger = false;
 *#if FSL_FEATURE_TPM_HAS_PAUSE_COUNTER_ON_TRIGGER
 *     config->enablePauseOnTrigger = false;
 *#endif
 *     config->triggerSelect = kTPM_Trigger_Select_0;
 *#if FSL_FEATURE_TPM_HAS_EXTERNAL_TRIGGER_SELECTION
 *     config->triggerSource = kTPM_TriggerSource_External;
 *#endif
 * endcode
 * param config Pointer to user's TPM config structure.
 */
void TPM_GetDefaultConfig(tpm_config_t *config)
{
    assert(NULL != config);

    /* Initializes the configure structure to zero. */
    (void)memset(config, 0, sizeof(*config));

    /* TPM clock divide by 1 */
    config->prescale = kTPM_Prescale_Divide_1;
#if !(defined(FSL_FEATURE_TPM_HAS_NO_CONF) && FSL_FEATURE_TPM_HAS_NO_CONF)
    /* Use internal TPM counter as timebase */
    config->useGlobalTimeBase = false;
    /* TPM counter continues in doze mode */
    config->enableDoze = false;
    /* TPM counter pauses when in debug mode */
    config->enableDebugMode = false;
    /* TPM counter will not be reloaded on input trigger */
    config->enableReloadOnTrigger = false;
    /* TPM counter continues running after overflow */
    config->enableStopOnOverflow = false;
    /* TPM counter starts immediately once it is enabled */
    config->enableStartOnTrigger = false;
#if defined(FSL_FEATURE_TPM_HAS_PAUSE_COUNTER_ON_TRIGGER) && FSL_FEATURE_TPM_HAS_PAUSE_COUNTER_ON_TRIGGER
    config->enablePauseOnTrigger = false;
#endif
    /* Choose trigger select 0 as input trigger for controlling counter operation */
    config->triggerSelect = kTPM_Trigger_Select_0;
#if defined(FSL_FEATURE_TPM_HAS_EXTERNAL_TRIGGER_SELECTION) && FSL_FEATURE_TPM_HAS_EXTERNAL_TRIGGER_SELECTION
    /* Choose external trigger source to control counter operation */
    config->triggerSource = kTPM_TriggerSource_External;
#endif
#endif
}

/*!
 * brief Configures the PWM signal parameters
 *
 * User calls this function to configure the PWM signals period, mode, dutycycle and edge. Use this
 * function to configure all the TPM channels that will be used to output a PWM signal
 *
 * param base        TPM peripheral base address
 * param chnlParams  Array of PWM channel parameters to configure the channel(s)
 * param numOfChnls  Number of channels to configure, this should be the size of the array passed in
 * param mode        PWM operation mode, options available in enumeration ::tpm_pwm_mode_t
 * param pwmFreq_Hz  PWM signal frequency in Hz
 * param srcClock_Hz TPM counter clock in Hz
 *
 * return kStatus_Success if the PWM setup was successful,
 *         kStatus_Error on failure
 */
status_t TPM_SetupPwm(TPM_Type *base,
                      const tpm_chnl_pwm_signal_param_t *chnlParams,
                      uint8_t numOfChnls,
                      tpm_pwm_mode_t mode,
                      uint32_t pwmFreq_Hz,
                      uint32_t srcClock_Hz)
{
    assert(NULL != chnlParams);
    assert(0U != pwmFreq_Hz);
    assert(0U != numOfChnls);
    assert(0U != srcClock_Hz);
#if defined(FSL_FEATURE_TPM_HAS_COMBINE) && FSL_FEATURE_TPM_HAS_COMBINE
    if (mode == kTPM_CombinedPwm)
    {
        assert(0 != FSL_FEATURE_TPM_COMBINE_HAS_EFFECTn(base));
    }
#endif

    uint32_t mod      = 0;
    uint32_t u32flag  = 1;
    uint32_t tpmClock = (srcClock_Hz / (u32flag << (base->SC & TPM_SC_PS_MASK)));
    uint16_t cnv;
    uint8_t i;

#if defined(FSL_FEATURE_TPM_HAS_QDCTRL) && FSL_FEATURE_TPM_HAS_QDCTRL
    /* The TPM's QDCTRL register required to be effective */
    if (0 != FSL_FEATURE_TPM_QDCTRL_HAS_EFFECTn(base))
    {
        /* Clear quadrature Decoder mode because in quadrature Decoder mode PWM doesn't operate*/
        base->QDCTRL &= ~TPM_QDCTRL_QUADEN_MASK;
    }
#endif

    switch (mode)
    {
        case kTPM_EdgeAlignedPwm:
#if defined(FSL_FEATURE_TPM_HAS_COMBINE) && FSL_FEATURE_TPM_HAS_COMBINE
        case kTPM_CombinedPwm:
#endif
            base->SC &= ~TPM_SC_CPWMS_MASK;
            mod = (tpmClock / pwmFreq_Hz) - 1u;
            break;
        case kTPM_CenterAlignedPwm:
            base->SC |= TPM_SC_CPWMS_MASK;
            mod = tpmClock / (pwmFreq_Hz * 2u);
            break;
        default:
            /* All the cease have been listed above, the default case should not be reached. */
            assert(false);
            break;
    }

    /* Return an error in case we overflow the registers, probably would require changing
     * clock source to get the desired frequency */
    if (mod > 65535U)
    {
        return kStatus_Fail;
    }
    /* Set the PWM period */
    base->MOD = mod;

    /* Setup each TPM channel */
    for (i = 0; i < numOfChnls; i++)
    {
        /* Return error if requested dutycycle is greater than the max allowed */
        if (chnlParams->dutyCyclePercent > 100U)
        {
            return kStatus_Fail;
        }
#if defined(FSL_FEATURE_TPM_HAS_COMBINE) && FSL_FEATURE_TPM_HAS_COMBINE
        if (mode == kTPM_CombinedPwm)
        {
            uint16_t cnvFirstEdge;

            /* This check is added for combined mode as the channel number should be the pair number */
            if ((int8_t)(chnlParams->chnlNumber) >= (FSL_FEATURE_TPM_CHANNEL_COUNTn(base) / 2))
            {
                return kStatus_Fail;
            }

            /* Return error if requested value is greater than the max allowed */
            if (chnlParams->firstEdgeDelayPercent > 100U)
            {
                return kStatus_Fail;
            }
            /* Configure delay of the first edge */
            if (chnlParams->firstEdgeDelayPercent == 0U)
            {
                /* No delay for the first edge */
                cnvFirstEdge = 0;
            }
            else
            {
                cnvFirstEdge = (uint16_t)((mod * chnlParams->firstEdgeDelayPercent) / 100U);
            }
            /* Configure dutycycle */
            if (chnlParams->dutyCyclePercent == 0U)
            {
                /* Signal stays low */
                cnv          = 0;
                cnvFirstEdge = 0;
            }
            else
            {
                cnv = (uint16_t)((mod * chnlParams->dutyCyclePercent) / 100U);
                /* For 100% duty cycle */
                if (cnv >= mod)
                {
                    cnv = (uint16_t)(mod + 1u);
                }
            }

            /* Set the combine bit for the channel pair */
            base->COMBINE |=
                (u32flag << (TPM_COMBINE_COMBINE0_SHIFT + (TPM_COMBINE_SHIFT * (uint32_t)chnlParams->chnlNumber)));

            /* When switching mode, disable channel n first */
            base->CONTROLS[(uint32_t)chnlParams->chnlNumber * 2U].CnSC &=
                ~(TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK);

            /* Wait till mode change to disable channel is acknowledged */
            while (0U != (base->CONTROLS[(uint32_t)chnlParams->chnlNumber * 2U].CnSC &
                          (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
            {
            }

            /* Set the requested PWM mode for channel n, PWM output requires mode select to be set to 2 */
            base->CONTROLS[(uint32_t)chnlParams->chnlNumber * 2U].CnSC |=
                (((uint32_t)chnlParams->level << TPM_CnSC_ELSA_SHIFT) | (2U << TPM_CnSC_MSA_SHIFT));

            /* Wait till mode change is acknowledged */
            while (0U == (base->CONTROLS[(uint32_t)chnlParams->chnlNumber * 2U].CnSC &
                          (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
            {
            }
            /* Set the channel pair values */
            base->CONTROLS[(uint16_t)chnlParams->chnlNumber * 2U].CnV = cnvFirstEdge;

            /* When switching mode, disable channel n + 1 first */
            base->CONTROLS[((uint32_t)chnlParams->chnlNumber * 2U) + 1U].CnSC &=
                ~(TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK);

            /* Wait till mode change to disable channel is acknowledged */
            while (0U != (base->CONTROLS[((uint32_t)chnlParams->chnlNumber * 2U) + 1U].CnSC &
                          (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
            {
            }

            /* Set the requested PWM mode for channel n + 1, PWM output requires mode select to be set to 2 */
            base->CONTROLS[((uint32_t)chnlParams->chnlNumber * 2U) + 1U].CnSC |=
                (((uint32_t)chnlParams->level << TPM_CnSC_ELSA_SHIFT) | (2U << TPM_CnSC_MSA_SHIFT));

            /* Wait till mode change is acknowledged */
            while (0U == (base->CONTROLS[((uint32_t)chnlParams->chnlNumber * 2U) + 1U].CnSC &
                          (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
            {
            }
            /* Set the channel pair values */
            base->CONTROLS[((uint16_t)chnlParams->chnlNumber * 2u) + 1u].CnV = (uint32_t)cnvFirstEdge + (uint32_t)cnv;
        }
        else
        {
#endif
            if (chnlParams->dutyCyclePercent == 0U)
            {
                /* Signal stays low */
                cnv = 0;
            }
            else
            {
                cnv = (uint16_t)((mod * chnlParams->dutyCyclePercent) / 100U);
                /* For 100% duty cycle */
                if (cnv >= mod)
                {
                    cnv = (uint16_t)(mod + 1U);
                }
            }
            /* Fix ERROR050050  When TPM is configured in EPWM mode as PS = 0, the compare event is missed on
            the first reload/overflow after writing 1 to the CnV register and causes an incorrect duty output.*/
#if (defined(FSL_FEATURE_TPM_HAS_ERRATA_050050) && FSL_FEATURE_TPM_HAS_ERRATA_050050)
            assert(
                !(mode == kTPM_EdgeAlignedPwm && cnv == 1U && (base->SC & TPM_SC_PS_MASK) == kTPM_Prescale_Divide_1));
#endif
            /* When switching mode, disable channel first */
            base->CONTROLS[chnlParams->chnlNumber].CnSC &=
                ~(TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK);

            /* Wait till mode change to disable channel is acknowledged */
            while (0U != (base->CONTROLS[chnlParams->chnlNumber].CnSC &
                          (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
            {
            }

            /* Set the requested PWM mode, PWM output requires mode select to be set to 2 */
            base->CONTROLS[chnlParams->chnlNumber].CnSC |=
                (((uint32_t)chnlParams->level << TPM_CnSC_ELSA_SHIFT) | (2U << TPM_CnSC_MSA_SHIFT));

            /* Wait till mode change is acknowledged */
            while (0U == (base->CONTROLS[chnlParams->chnlNumber].CnSC &
                          (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
            {
            }
            base->CONTROLS[chnlParams->chnlNumber].CnV = cnv;
#if defined(FSL_FEATURE_TPM_HAS_COMBINE) && FSL_FEATURE_TPM_HAS_COMBINE
        }
#endif

        chnlParams++;
    }

    return kStatus_Success;
}

/*!
 * brief Update the duty cycle of an active PWM signal
 *
 * param base              TPM peripheral base address
 * param chnlNumber        The channel number. In combined mode, this represents
 *                          the channel pair number
 * param currentPwmMode    The current PWM mode set during PWM setup
 * param dutyCyclePercent  New PWM pulse width, value should be between 0 to 100
 *                          0=inactive signal(0% duty cycle)...
 *                          100=active signal (100% duty cycle)
 */
void TPM_UpdatePwmDutycycle(TPM_Type *base,
                            tpm_chnl_t chnlNumber,
                            tpm_pwm_mode_t currentPwmMode,
                            uint8_t dutyCyclePercent)
{
    assert((int8_t)chnlNumber < FSL_FEATURE_TPM_CHANNEL_COUNTn(base));
    assert((uint8_t)chnlNumber < sizeof(base->CONTROLS) / sizeof(base->CONTROLS[0]));

#if defined(FSL_FEATURE_TPM_HAS_COMBINE) && FSL_FEATURE_TPM_HAS_COMBINE
    if (currentPwmMode == kTPM_CombinedPwm)
    {
        assert(0 != FSL_FEATURE_TPM_COMBINE_HAS_EFFECTn(base));
    }
#endif

    uint16_t cnv, mod;

    mod = (uint16_t)(base->MOD);
#if defined(FSL_FEATURE_TPM_HAS_COMBINE) && FSL_FEATURE_TPM_HAS_COMBINE
    if (currentPwmMode == kTPM_CombinedPwm)
    {
        uint16_t cnvFirstEdge;

        /* This check is added for combined mode as the channel number should be the pair number */
        if ((int8_t)chnlNumber >= ((int8_t)FSL_FEATURE_TPM_CHANNEL_COUNTn(base) / 2))
        {
            return;
        }
        cnv          = (uint16_t)((mod * dutyCyclePercent) / 100U);
        cnvFirstEdge = (uint16_t)(base->CONTROLS[((uint8_t)chnlNumber * 2u) & 0x3U].CnV);
        /* For 100% duty cycle */
        if (cnv >= mod)
        {
            cnv = mod + 1u;
        }
        base->CONTROLS[(((uint8_t)chnlNumber * 2u) + 1u) & 0x3U].CnV = (uint32_t)cnvFirstEdge + (uint32_t)cnv;
    }
    else
    {
#endif
        cnv = (uint16_t)((mod * dutyCyclePercent) / 100U);
        /* For 100% duty cycle */
        if (cnv >= mod)
        {
            cnv = mod + 1u;
        }
        /* Fix ERROR050050 */
#if (defined(FSL_FEATURE_TPM_HAS_ERRATA_050050) && FSL_FEATURE_TPM_HAS_ERRATA_050050)
        assert(!(currentPwmMode == kTPM_EdgeAlignedPwm && cnv == 1U &&
                 (base->SC & TPM_SC_PS_MASK) == kTPM_Prescale_Divide_1));
#endif
        base->CONTROLS[chnlNumber].CnV = cnv;
#if defined(FSL_FEATURE_TPM_WAIT_CnV_REGISTER_UPDATE) && FSL_FEATURE_TPM_WAIT_CnV_REGISTER_UPDATE
        while (!(cnv == base->CONTROLS[chnlNumber].CnV))
        {
        }
#endif

#if defined(FSL_FEATURE_TPM_HAS_COMBINE) && FSL_FEATURE_TPM_HAS_COMBINE
    }
#endif
}

/*!
 * brief Update the edge level selection for a channel
 *
 * param base       TPM peripheral base address
 * param chnlNumber The channel number
 * param level      The level to be set to the ELSnB:ELSnA field; valid values are 00, 01, 10, 11.
 *                   See the appropriate SoC reference manual for details about this field.
 */
void TPM_UpdateChnlEdgeLevelSelect(TPM_Type *base, tpm_chnl_t chnlNumber, uint8_t level)
{
    assert((int8_t)chnlNumber < FSL_FEATURE_TPM_CHANNEL_COUNTn(base));
    assert((uint8_t)chnlNumber < sizeof(base->CONTROLS) / sizeof(base->CONTROLS[0]));

    uint32_t reg = base->CONTROLS[chnlNumber].CnSC
#if !(defined(FSL_FEATURE_TPM_CnSC_CHF_WRITE_0_CLEAR) && FSL_FEATURE_TPM_CnSC_CHF_WRITE_0_CLEAR)
                   & ~(TPM_CnSC_CHF_MASK)
#endif
        ;

    /* When switching mode, disable channel first  */
    base->CONTROLS[chnlNumber].CnSC &=
        ~(TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK);

    /* Wait till mode change to disable channel is acknowledged */
    while (0U != (base->CONTROLS[chnlNumber].CnSC &
                  (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
    {
    }

    /* Clear the field and write the new level value */
    reg &= ~(TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK);
    reg |= ((uint32_t)level << TPM_CnSC_ELSA_SHIFT) & (TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK);

    base->CONTROLS[chnlNumber].CnSC = reg;

    /* Wait till mode change is acknowledged */
    reg &= (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK);
    while (reg != (base->CONTROLS[chnlNumber].CnSC &
                   (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
    {
    }
}

/*!
 * brief Enables capturing an input signal on the channel using the function parameters.
 *
 * When the edge specified in the captureMode argument occurs on the channel, the TPM counter is captured into
 * the CnV register. The user has to read the CnV register separately to get this value.
 *
 * param base        TPM peripheral base address
 * param chnlNumber  The channel number
 * param captureMode Specifies which edge to capture
 */
void TPM_SetupInputCapture(TPM_Type *base, tpm_chnl_t chnlNumber, tpm_input_capture_edge_t captureMode)
{
    assert((int8_t)chnlNumber < FSL_FEATURE_TPM_CHANNEL_COUNTn(base));
    assert((uint8_t)chnlNumber < sizeof(base->CONTROLS) / sizeof(base->CONTROLS[0]));

#if defined(FSL_FEATURE_TPM_HAS_QDCTRL) && FSL_FEATURE_TPM_HAS_QDCTRL
    /* The TPM's QDCTRL register required to be effective */
    if (0 != FSL_FEATURE_TPM_QDCTRL_HAS_EFFECTn(base))
    {
        /* Clear quadrature Decoder mode for channel 0 or 1*/
        if (((uint32_t)chnlNumber == 0u) || ((uint32_t)chnlNumber == 1u))
        {
            base->QDCTRL &= ~TPM_QDCTRL_QUADEN_MASK;
        }
    }
#endif

#if defined(FSL_FEATURE_TPM_HAS_COMBINE) && FSL_FEATURE_TPM_HAS_COMBINE
    /* The TPM's COMBINE register required to be effective */
    if (0 != FSL_FEATURE_TPM_COMBINE_HAS_EFFECTn(base))
    {
        /* Clear the combine bit for chnlNumber */
        base->COMBINE &= ~((uint32_t)1U << (((uint32_t)chnlNumber / 2U) * TPM_COMBINE_SHIFT));
    }
#endif

    /* When switching mode, disable channel first  */
    base->CONTROLS[chnlNumber].CnSC &=
        ~(TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK);

    /* Wait till mode change to disable channel is acknowledged */
    while (0U != (base->CONTROLS[chnlNumber].CnSC &
                  (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
    {
    }

    /* Set the requested input capture mode */
    base->CONTROLS[chnlNumber].CnSC |= (uint32_t)captureMode;

    /* Wait till mode change is acknowledged */
    while (0U == (base->CONTROLS[chnlNumber].CnSC &
                  (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
    {
    }
}

/*!
 * brief Configures the TPM to generate timed pulses.
 *
 * When the TPM counter matches the value of compareVal argument (this is written into CnV reg), the channel
 * output is changed based on what is specified in the compareMode argument.
 *
 * param base         TPM peripheral base address
 * param chnlNumber   The channel number
 * param compareMode  Action to take on the channel output when the compare condition is met
 * param compareValue Value to be programmed in the CnV register.
 */
void TPM_SetupOutputCompare(TPM_Type *base,
                            tpm_chnl_t chnlNumber,
                            tpm_output_compare_mode_t compareMode,
                            uint32_t compareValue)
{
    assert((int8_t)chnlNumber < FSL_FEATURE_TPM_CHANNEL_COUNTn(base));
    assert((uint8_t)chnlNumber < sizeof(base->CONTROLS) / sizeof(base->CONTROLS[0]));

#if defined(FSL_FEATURE_TPM_HAS_QDCTRL) && FSL_FEATURE_TPM_HAS_QDCTRL
    /* The TPM's QDCTRL register required to be effective */
    if (0 != FSL_FEATURE_TPM_QDCTRL_HAS_EFFECTn(base))
    {
        /* Clear quadrature Decoder mode for channel 0 or 1 */
        if (((uint32_t)chnlNumber == 0U) || ((uint32_t)chnlNumber == 1U))
        {
            base->QDCTRL &= ~TPM_QDCTRL_QUADEN_MASK;
        }
    }
#endif

    /* When switching mode, disable channel first  */
    base->CONTROLS[chnlNumber].CnSC &=
        ~(TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK);

    /* Wait till mode change to disable channel is acknowledged */
    while (0U != (base->CONTROLS[chnlNumber].CnSC &
                  (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
    {
    }

    /* Setup the channel output behaviour when a match occurs with the compare value */
    base->CONTROLS[chnlNumber].CnSC |= (uint32_t)compareMode;

    /* Setup the compare value */
    base->CONTROLS[chnlNumber].CnV = compareValue;

    /* Wait till mode change is acknowledged */
    while (0U == (base->CONTROLS[chnlNumber].CnSC &
                  (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
    {
    }
}

#if defined(FSL_FEATURE_TPM_HAS_COMBINE) && FSL_FEATURE_TPM_HAS_COMBINE
/*!
 * brief Configures the dual edge capture mode of the TPM.
 *
 * This function allows to measure a pulse width of the signal on the input of channel of a
 * channel pair. The filter function is disabled if the filterVal argument passed is zero.
 *
 * param base           TPM peripheral base address
 * param chnlPairNumber The TPM channel pair number; options are 0, 1, 2, 3
 * param edgeParam      Sets up the dual edge capture function
 * param filterValue    Filter value, specify 0 to disable filter.
 */
void TPM_SetupDualEdgeCapture(TPM_Type *base,
                              tpm_chnl_t chnlPairNumber,
                              const tpm_dual_edge_capture_param_t *edgeParam,
                              uint32_t filterValue)
{
    assert(NULL != edgeParam);
    assert((int8_t)chnlPairNumber < (int8_t)FSL_FEATURE_TPM_CHANNEL_COUNTn(base) / 2);
    assert(0 != FSL_FEATURE_TPM_COMBINE_HAS_EFFECTn(base));

    uint32_t reg;
    uint32_t u32flag;

#if defined(FSL_FEATURE_TPM_HAS_QDCTRL) && FSL_FEATURE_TPM_HAS_QDCTRL
    /* The TPM's QDCTRL register required to be effective */
    if (0 != FSL_FEATURE_TPM_QDCTRL_HAS_EFFECTn(base))
    {
        /* Clear quadrature Decoder mode for channel 0 or 1*/
        if ((uint32_t)chnlPairNumber == 0u)
        {
            base->QDCTRL &= ~TPM_QDCTRL_QUADEN_MASK;
        }
    }
#endif

    /* Unlock: When switching mode, disable channel first */
    base->CONTROLS[(uint32_t)chnlPairNumber * 2U].CnSC &=
        ~(TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK);

    /* Wait till mode change to disable channel is acknowledged */
    while (0U != (base->CONTROLS[(uint32_t)chnlPairNumber * 2U].CnSC &
                  (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
    {
    }

    base->CONTROLS[((uint32_t)chnlPairNumber * 2U) + 1U].CnSC &=
        ~(TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK);

    /* Wait till mode change to disable channel is acknowledged */
    while (0U != (base->CONTROLS[((uint32_t)chnlPairNumber * 2U) + 1U].CnSC &
                  (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
    {
    }

    /* Now, the registers for input mode can be operated. */
    if (true == edgeParam->enableSwap)
    {
        u32flag = TPM_COMBINE_COMBINE0_MASK | TPM_COMBINE_COMSWAP0_MASK;
        /* Set the combine and swap bits for the channel pair */
        base->COMBINE |= u32flag << (TPM_COMBINE_SHIFT * (uint32_t)chnlPairNumber);

        /* Input filter setup for channel n+1 input */
        reg = base->FILTER;
        reg &= ~((uint32_t)TPM_FILTER_CH0FVAL_MASK << (TPM_FILTER_CH1FVAL_SHIFT * ((uint32_t)chnlPairNumber + 1U)));
        reg |= (filterValue << (TPM_FILTER_CH1FVAL_SHIFT * ((uint32_t)chnlPairNumber + 1U)));
        base->FILTER = reg;
    }
    else
    {
        reg = base->COMBINE;
        /* Clear the swap bit for the channel pair */
        reg &= ~((uint32_t)TPM_COMBINE_COMSWAP0_MASK << ((uint32_t)chnlPairNumber * TPM_COMBINE_COMSWAP0_SHIFT));
        u32flag = TPM_COMBINE_COMBINE0_MASK;

        /* Set the combine bit for the channel pair */
        reg |= u32flag << (TPM_COMBINE_SHIFT * (uint32_t)chnlPairNumber);
        base->COMBINE = reg;

        /* Input filter setup for channel n input */
        reg = base->FILTER;
        reg &= ~((uint32_t)TPM_FILTER_CH0FVAL_MASK << (TPM_FILTER_CH1FVAL_SHIFT * (uint32_t)chnlPairNumber));
        reg |= (filterValue << (TPM_FILTER_CH1FVAL_SHIFT * (uint32_t)chnlPairNumber));
        base->FILTER = reg;
    }

    /* Setup the edge detection from channel n */
    base->CONTROLS[(uint32_t)chnlPairNumber * 2u].CnSC |= (uint32_t)edgeParam->currChanEdgeMode;

    /* Wait till mode change is acknowledged */
    while (0U == (base->CONTROLS[(uint32_t)chnlPairNumber * 2U].CnSC &
                  (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
    {
    }

    /* Setup the edge detection from channel n+1 */
    base->CONTROLS[((uint32_t)chnlPairNumber * 2U) + 1U].CnSC |= (uint32_t)edgeParam->nextChanEdgeMode;

    /* Wait till mode change is acknowledged */
    while (0U == (base->CONTROLS[((uint32_t)chnlPairNumber * 2U) + 1U].CnSC &
                  (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
    {
    }
}
#endif

#if defined(FSL_FEATURE_TPM_HAS_QDCTRL) && FSL_FEATURE_TPM_HAS_QDCTRL
/*!
 * brief Configures the parameters and activates the quadrature decode mode.
 *
 * param base         TPM peripheral base address
 * param phaseAParams Phase A configuration parameters
 * param phaseBParams Phase B configuration parameters
 * param quadMode     Selects encoding mode used in quadrature decoder mode
 */
void TPM_SetupQuadDecode(TPM_Type *base,
                         const tpm_phase_params_t *phaseAParams,
                         const tpm_phase_params_t *phaseBParams,
                         tpm_quad_decode_mode_t quadMode)
{
    assert(NULL != phaseAParams);
    assert(NULL != phaseBParams);
    assert(0 != FSL_FEATURE_TPM_QDCTRL_HAS_EFFECTn(base));

    base->CONTROLS[0].CnSC &= ~(TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK);

    /* Wait till mode change to disable channel is acknowledged */
    while (0U !=
           (base->CONTROLS[0].CnSC & (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
    {
    }
    uint32_t reg;

    /* Set Phase A filter value */
    reg = base->FILTER;
    reg &= ~(TPM_FILTER_CH0FVAL_MASK);
    reg |= TPM_FILTER_CH0FVAL(phaseAParams->phaseFilterVal);
    base->FILTER = reg;

#if defined(FSL_FEATURE_TPM_HAS_POL) && FSL_FEATURE_TPM_HAS_POL
    /* Set Phase A polarity */
    if (kTPM_QuadPhaseInvert == phaseAParams->phasePolarity)
    {
        base->POL |= TPM_POL_POL0_MASK;
    }
    else
    {
        base->POL &= ~TPM_POL_POL0_MASK;
    }
#endif

    base->CONTROLS[1].CnSC &= ~(TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK);

    /* Wait till mode change to disable channel is acknowledged */
    while (0U !=
           (base->CONTROLS[1].CnSC & (TPM_CnSC_MSA_MASK | TPM_CnSC_MSB_MASK | TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK)))
    {
    }
    /* Set Phase B filter value */
    reg = base->FILTER;
    reg &= ~(TPM_FILTER_CH1FVAL_MASK);
    reg |= TPM_FILTER_CH1FVAL(phaseBParams->phaseFilterVal);
    base->FILTER = reg;
#if defined(FSL_FEATURE_TPM_HAS_POL) && FSL_FEATURE_TPM_HAS_POL
    /* Set Phase B polarity */
    if (kTPM_QuadPhaseInvert == phaseBParams->phasePolarity)
    {
        base->POL |= TPM_POL_POL1_MASK;
    }
    else
    {
        base->POL &= ~TPM_POL_POL1_MASK;
    }
#endif

    /* Set Quadrature mode */
    reg = base->QDCTRL;
    reg &= ~(TPM_QDCTRL_QUADMODE_MASK);
    reg |= TPM_QDCTRL_QUADMODE(quadMode);
    base->QDCTRL = reg;

    /* Enable Quad decode */
    base->QDCTRL |= TPM_QDCTRL_QUADEN_MASK;
}

#endif

/*!
 * brief Enables the selected TPM interrupts.
 *
 * param base TPM peripheral base address
 * param mask The interrupts to enable. This is a logical OR of members of the
 *             enumeration ::tpm_interrupt_enable_t
 */
void TPM_EnableInterrupts(TPM_Type *base, uint32_t mask)
{
    uint32_t chnlInterrupts = (mask & 0xFFU);
    uint8_t chnlNumber      = 0;

    /* Enable the timer overflow interrupt */
    if ((uint32_t)kTPM_TimeOverflowInterruptEnable == (mask & (uint32_t)kTPM_TimeOverflowInterruptEnable))
    {
        base->SC |= TPM_SC_TOIE_MASK;
    }

    /* Enable the channel interrupts */
    while (0U != chnlInterrupts)
    {
        if (0U != (chnlInterrupts & 0x1u))
        {
            base->CONTROLS[chnlNumber].CnSC |= TPM_CnSC_CHIE_MASK;
        }
        chnlNumber++;
        chnlInterrupts = chnlInterrupts >> 1U;
    }
}

/*!
 * brief Disables the selected TPM interrupts.
 *
 * param base TPM peripheral base address
 * param mask The interrupts to disable. This is a logical OR of members of the
 *             enumeration ::tpm_interrupt_enable_t
 */
void TPM_DisableInterrupts(TPM_Type *base, uint32_t mask)
{
    uint32_t chnlInterrupts = (mask & 0xFFU);
    uint8_t chnlNumber      = 0;

    /* Disable the timer overflow interrupt */
    if ((uint32_t)kTPM_TimeOverflowInterruptEnable == (mask & (uint32_t)kTPM_TimeOverflowInterruptEnable))
    {
        base->SC &= ~TPM_SC_TOIE_MASK;
    }

    /* Disable the channel interrupts */
    while (0U != chnlInterrupts)
    {
        if (0U != (chnlInterrupts & 0x1u))
        {
            base->CONTROLS[chnlNumber].CnSC &= ~TPM_CnSC_CHIE_MASK;
        }
        chnlNumber++;
        chnlInterrupts = chnlInterrupts >> 1U;
    }
}

/*!
 * brief Gets the enabled TPM interrupts.
 *
 * param base TPM peripheral base address
 *
 * return The enabled interrupts. This is the logical OR of members of the
 *         enumeration ::tpm_interrupt_enable_t
 */
uint32_t TPM_GetEnabledInterrupts(TPM_Type *base)
{
    uint32_t enabledInterrupts = 0;
    uint32_t u32flag           = 1;
    int8_t chnlCount           = FSL_FEATURE_TPM_CHANNEL_COUNTn(base);

    /* The CHANNEL_COUNT macro returns -1 if it cannot match the TPM instance */
    assert(chnlCount != -1);

    /* Check if timer overflow interrupt is enabled */
    if (0U != (base->SC & TPM_SC_TOIE_MASK))
    {
        enabledInterrupts |= (uint32_t)kTPM_TimeOverflowInterruptEnable;
    }

    /* Check if the channel interrupts are enabled */
    while (chnlCount > 0)
    {
        chnlCount--;
        if (0U != (base->CONTROLS[chnlCount].CnSC & TPM_CnSC_CHIE_MASK))
        {
            enabledInterrupts |= (u32flag << (uint8_t)chnlCount);
        }
    }

    return enabledInterrupts;
}
