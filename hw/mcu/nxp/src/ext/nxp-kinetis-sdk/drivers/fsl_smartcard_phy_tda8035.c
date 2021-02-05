/*
 * Copyright (c) 2015-2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_smartcard_phy.h"
#if (defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT))
#include "fsl_smartcard_emvsim.h"
#else
#include "fsl_smartcard_uart.h"
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* Component ID definition, used by tools. */
#ifndef FSL_COMPONENT_ID
#define FSL_COMPONENT_ID "platform.drivers.smartcard_phy_tda8035"
#endif

/*! @brief Masks for TDA8035 status register */
#define SMARTCARD_TDA8035_STATUS_PRES             (0x01u) /*!< Smart card PHY TDA8035 Smart card present status */
#define SMARTCARD_TDA8035_STATUS_ACTIVE           (0x02u) /*!< Smart card PHY TDA8035 Smart card active status */
#define SMARTCARD_TDA8035_STATUS_FAULTY           (0x04u) /*!< Smart card PHY TDA8035 Smart card faulty status */
#define SMARTCARD_TDA8035_STATUS_CARD_REMOVED     (0x08u) /*!< Smart card PHY TDA8035 Smart card removed status */
#define SMARTCARD_TDA8035_STATUS_CARD_DEACTIVATED (0x10u) /*!< Smart card PHY TDA8035 Smart card deactivated status */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static uint32_t smartcard_phy_tda8035_InterfaceClockInit(void *base,
                                                         smartcard_interface_config_t const *config,
                                                         uint32_t srcClock_Hz);
static void smartcard_phy_tda8035_InterfaceClockDeinit(void *base, smartcard_interface_config_t const *config);
static void smartcard_phy_tda8035_InterfaceClockEnable(void *base, smartcard_interface_config_t const *config);

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/

/*!
 * @brief This function initializes clock module used for card clock generation
 */
static uint32_t smartcard_phy_tda8035_InterfaceClockInit(void *base,
                                                         smartcard_interface_config_t const *config,
                                                         uint32_t srcClock_Hz)
{
    assert((NULL != config));
#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT)
    assert(config->clockModule < (uint8_t)FSL_FEATURE_SOC_EMVSIM_COUNT);

    uint32_t emvsimClkMhz = 0u;
    uint8_t emvsimPRSCValue;

    /* Retrieve EMV SIM clock */
    emvsimClkMhz = srcClock_Hz / 1000000u;
    /* Calculate MOD value */
    emvsimPRSCValue = (uint8_t)((emvsimClkMhz * 1000u) / (config->smartCardClock / 1000u));
    /* Set clock prescaler */
    ((EMVSIM_Type *)base)->CLKCFG =
        (((EMVSIM_Type *)base)->CLKCFG & ~EMVSIM_CLKCFG_CLK_PRSC_MASK) | EMVSIM_CLKCFG_CLK_PRSC(emvsimPRSCValue);

    return config->smartCardClock;
#elif defined(FSL_FEATURE_SOC_FTM_COUNT) && (FSL_FEATURE_SOC_FTM_COUNT)
    assert(config->clockModule < FSL_FEATURE_SOC_FTM_COUNT);

    uint32_t periph_clk_mhz = 0u;
    uint16_t ftmModValue;
    uint32_t ftm_base[] = FTM_BASE_ADDRS;
    FTM_Type *ftmBase   = (FTM_Type *)ftm_base[config->clockModule];

    /* Retrieve FTM system clock */
    periph_clk_mhz = srcClock_Hz / 1000000u;
    /* Calculate MOD value */
    ftmModValue = ((periph_clk_mhz * 1000u / 2u) / (config->smartCardClock / 1000u)) - 1u;
    /* un-gate FTM peripheral clock */
    switch (config->clockModule)
    {
        case 0u:
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
            CLOCK_EnableClock(kCLOCK_Ftm0);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
            break;
#if FSL_FEATURE_SOC_FTM_COUNT > 1
        case 1u:
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
            CLOCK_EnableClock(kCLOCK_Ftm1);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
            break;
#endif
#if FSL_FEATURE_SOC_FTM_COUNT > 2
        case 2u:
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
            CLOCK_EnableClock(kCLOCK_Ftm2);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
            break;
#endif
#if FSL_FEATURE_SOC_FTM_COUNT > 3
        case 3u:
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
            CLOCK_EnableClock(kCLOCK_Ftm3);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
            break;
#endif
        default:
            return 0u;
    }
    /* Initialize FTM driver */
    /* Reset FTM prescaler to 'Divide by 1', i.e., to be same clock as peripheral clock
     * Disable FTM counter, Set counter to operates in Up-counting mode */
    ftmBase->SC &= ~(FTM_SC_PS_MASK | FTM_SC_CLKS_MASK | FTM_SC_CPWMS_MASK);
    /* Set initial counter value */
    ftmBase->CNTIN = 0u;
    /* Set MOD value */
    ftmBase->MOD = ftmModValue;
    /* Configure mode to output compare, toggle output on match */
    ftmBase->CONTROLS[config->clockModuleChannel].CnSC = (FTM_CnSC_ELSA_MASK | FTM_CnSC_MSA_MASK);
    /* Configure a match value to toggle output at */
    ftmBase->CONTROLS[config->clockModuleChannel].CnV = 1;
    /* Re-calculate the actually configured smartcard clock and return to caller */
    return (uint32_t)(((periph_clk_mhz * 1000u / 2u) / (ftmBase->MOD + 1u)) * 1000u);
#else
    return 0u;
#endif
}

/*!
 * @brief This function de-initialize clock module used for card clock generation
 */
static void smartcard_phy_tda8035_InterfaceClockDeinit(void *base, smartcard_interface_config_t const *config)
{
#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT)
    assert(((config->clockModule < (uint8_t)FSL_FEATURE_SOC_EMVSIM_COUNT)) && (NULL != base));

    /* Disable smart card clock */
    ((EMVSIM_Type *)base)->PCSR &= ~EMVSIM_PCSR_SCEN_MASK;
#elif defined(FSL_FEATURE_SOC_FTM_COUNT) && (FSL_FEATURE_SOC_FTM_COUNT)
    assert(config->clockModule < FSL_FEATURE_SOC_FTM_COUNT);
    /* gate FTM peripheral clock */
    switch (config->clockModule)
    {
        case 0u:
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
            CLOCK_DisableClock(kCLOCK_Ftm0);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
            break;
#if FSL_FEATURE_SOC_FTM_COUNT > 1
        case 1u:
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
            CLOCK_DisableClock(kCLOCK_Ftm1);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
            break;
#endif
#if FSL_FEATURE_SOC_FTM_COUNT > 2
        case 2u:
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
            CLOCK_DisableClock(kCLOCK_Ftm2);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
            break;
#endif
#if FSL_FEATURE_SOC_FTM_COUNT > 3
        case 3u:
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
            CLOCK_DisableClock(kCLOCK_Ftm3);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
            break;
#endif
        default:
            assert(false);
            break;
    }
#endif
}

/*!
 * @brief This function activate smart card clock
 */
static void smartcard_phy_tda8035_InterfaceClockEnable(void *base, smartcard_interface_config_t const *config)
{
/* Enable smart card clock */
#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT)
    ((EMVSIM_Type *)base)->PCSR |= EMVSIM_PCSR_SCEN_MASK;
#elif defined(FSL_FEATURE_SOC_FTM_COUNT) && (FSL_FEATURE_SOC_FTM_COUNT)
    uint32_t ftm_base[] = FTM_BASE_ADDRS;
    FTM_Type *ftmBase   = (FTM_Type *)ftm_base[config->clockModule];
    /* Set clock source to start the counter : System clock */
    ftmBase->SC         = FTM_SC_CLKS(1);
#endif
}

/*!
 * @brief This function deactivate smart card clock
 */
static void smartcard_phy_tda8035_InterfaceClockDisable(void *base, smartcard_interface_config_t const *config)
{
/* Enable smart card clock */
#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT)
    ((EMVSIM_Type *)base)->PCSR &= ~EMVSIM_PCSR_SCEN_MASK;
#elif defined(FSL_FEATURE_SOC_FTM_COUNT) && (FSL_FEATURE_SOC_FTM_COUNT)
    uint32_t ftm_base[] = FTM_BASE_ADDRS;
    FTM_Type *ftmBase   = (FTM_Type *)ftm_base[config->clockModule];
    /* Set clock source to start the counter : System clock */
    ftmBase->SC &= ~FTM_SC_CLKS_MASK;
#endif
}

void SMARTCARD_PHY_GetDefaultConfig(smartcard_interface_config_t *config)
{
    assert((NULL != config));

    /* Initializes the configure structure to zero. */
    (void)memset(config, 0, sizeof(*config));

    config->clockToResetDelay = SMARTCARD_INIT_DELAY_CLOCK_CYCLES;
    config->vcc               = kSMARTCARD_VoltageClassB3_3V;
}

status_t SMARTCARD_PHY_Init(void *base, smartcard_interface_config_t const *config, uint32_t srcClock_Hz)
{
    if ((NULL == config) || (0u == srcClock_Hz))
    {
        return kStatus_SMARTCARD_InvalidInput;
    }

    /* Configure GPIO(CMDVCC, RST, INT, VSEL0, VSEL1) pins */
    uint32_t gpio_base[] = GPIO_BASE_ADDRS;
    IRQn_Type port_irq[] = PORT_IRQS;
    /* Set VSEL pins to low level context */
    ((GPIO_Type *)gpio_base[config->vsel0Port])->PCOR |= ((uint32_t)1u << config->vsel0Pin);
    ((GPIO_Type *)gpio_base[config->vsel1Port])->PCOR |= ((uint32_t)1u << config->vsel1Pin);
    /* Set VSEL pins to output pins */
    ((GPIO_Type *)gpio_base[config->vsel0Port])->PDDR |= ((uint32_t)1u << config->vsel0Pin);
    ((GPIO_Type *)gpio_base[config->vsel1Port])->PDDR |= ((uint32_t)1u << config->vsel1Pin);

    /* vcc = 5v: vsel0=1,vsel1=1
     * vcc = 3.3v: vsel0=0,vsel1=1
     * vcc = 1.8v: vsel0=x,vsel1=0 */
    /* Setting of VSEL1 pin */
    if ((kSMARTCARD_VoltageClassA5_0V == config->vcc) || (kSMARTCARD_VoltageClassB3_3V == config->vcc))
    {
        ((GPIO_Type *)gpio_base[config->vsel1Port])->PSOR |= ((uint32_t)1u << config->vsel1Pin);
    }
    else
    {
        ((GPIO_Type *)gpio_base[config->vsel1Port])->PCOR |= ((uint32_t)1u << config->vsel1Pin);
    }
    /* Setting of VSEL0 pin */
    if (kSMARTCARD_VoltageClassA5_0V == config->vcc)
    {
        ((GPIO_Type *)gpio_base[config->vsel0Port])->PSOR |= ((uint32_t)1u << config->vsel0Pin);
    }
    else
    {
        ((GPIO_Type *)gpio_base[config->vsel0Port])->PCOR |= ((uint32_t)1u << config->vsel0Pin);
    }

#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT)
    /* Set CMD_VCC pin to logic level '1', to allow card detection interrupt from TDA8035 */
    ((EMVSIM_Type *)base)->PCSR |= EMVSIM_PCSR_SVCC_EN_MASK;
    ((EMVSIM_Type *)base)->PCSR &= ~EMVSIM_PCSR_VCCENP_MASK;
#else
    /* Set RST pin to zero context and CMDVCC to high context */
    ((GPIO_Type *)gpio_base[config->resetPort])->PCOR |= ((uint32_t)1u << config->resetPin);
    ((GPIO_Type *)gpio_base[config->controlPort])->PSOR |= ((uint32_t)1u << config->controlPin);
    /* Set CMDVCC, RESET pins as output pins */
    ((GPIO_Type *)gpio_base[config->resetPort])->PDDR |= ((uint32_t)1u << config->resetPin);
    ((GPIO_Type *)gpio_base[config->controlPort])->PDDR |= ((uint32_t)1u << config->controlPin);

#endif
    /* Initialize INT pin */
    ((GPIO_Type *)gpio_base[config->irqPort])->PDDR &= ~((uint32_t)1u << config->irqPin);
    /* Enable Port IRQ for smartcard presence detection */
    NVIC_EnableIRQ(port_irq[config->irqPort]);
    /* Smartcard clock initialization */
    if (config->smartCardClock != smartcard_phy_tda8035_InterfaceClockInit(base, config, srcClock_Hz))
    {
        return kStatus_SMARTCARD_OtherError;
    }

    return kStatus_SMARTCARD_Success;
}

void SMARTCARD_PHY_Deinit(void *base, smartcard_interface_config_t const *config)
{
    assert((NULL != config));

    IRQn_Type port_irq[] = PORT_IRQS;
    NVIC_DisableIRQ(port_irq[config->irqPort]);
    /* Stop smartcard clock */
    smartcard_phy_tda8035_InterfaceClockDeinit(base, config);
}

status_t SMARTCARD_PHY_Activate(void *base, smartcard_context_t *context, smartcard_reset_type_t resetType)
{
    if ((NULL == context) || (NULL == context->timeDelay))
    {
        return kStatus_SMARTCARD_InvalidInput;
    }

    context->timersState.initCharTimerExpired = false;
    context->resetType                        = resetType;
#if !(defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT))
    uint32_t gpio_base[] = GPIO_BASE_ADDRS;
#endif
#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT)
    EMVSIM_Type *emvsimBase = (EMVSIM_Type *)base;
    /* Disable receiver to deactivate GPC timers trigger */
    emvsimBase->CTRL &= ~EMVSIM_CTRL_RCV_EN_MASK;
#endif

    if (resetType == kSMARTCARD_ColdReset)
    { /* Ensure that RST is HIGH and CMD is high here so that PHY goes in normal mode */
#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT)
        emvsimBase->PCSR =
            (emvsimBase->PCSR & ~(EMVSIM_PCSR_VCCENP_MASK | EMVSIM_PCSR_SRST_MASK)) | EMVSIM_PCSR_SVCC_EN_MASK;
        emvsimBase->PCSR &= ~EMVSIM_PCSR_SRST_MASK;
#else
        ((GPIO_Type *)gpio_base[context->interfaceConfig.resetPort])->PCOR |= (1u << context->interfaceConfig.resetPin);
        ((GPIO_Type *)gpio_base[context->interfaceConfig.controlPort])->PSOR |=
            (1u << context->interfaceConfig.controlPin);
#endif

/* Set PHY to start Activation sequence by pulling CMDVCC low */
#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT)
        emvsimBase->PCSR |= EMVSIM_PCSR_VCCENP_MASK;
#else
        ((GPIO_Type *)gpio_base[context->interfaceConfig.controlPort])->PCOR |=
            (1u << context->interfaceConfig.controlPin);
#endif
        /* wait 3.42ms then enable clock  an10997 P29
        During t0, the TDA8035 checks for the XTAL1 pin to detect if a crystal is present or if the clock is supplied
        from the micro-controller, and then waits for the crystal to start.
        This time is fixed, even if there is no crystal, and its maximum value is 3.1 ms.
        t1 is the time between the beginning of the activation and the start of the clock on the smart card side. This
        time depends on the internal oscillator frequency and
        lasts at maximum 320 us. */

        /* Set counter value , no card clock ,so use OS delay */
        context->timeDelay(3500u);
        smartcard_phy_tda8035_InterfaceClockEnable(base, &context->interfaceConfig);
    }
    else if (resetType == kSMARTCARD_WarmReset)
    { /* Ensure that card is already active */
        if (!context->cardParams.active)
        { /* Card is not active;hence return */
            return kStatus_SMARTCARD_CardNotActivated;
        }
/* Pull RESET low to start warm Activation sequence */
#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT)
        emvsimBase->PCSR &= ~EMVSIM_PCSR_SRST_MASK;
#else
        ((GPIO_Type *)gpio_base[context->interfaceConfig.resetPort])->PCOR |= (1u << context->interfaceConfig.resetPin);
#endif
    }
    else
    {
        return kStatus_SMARTCARD_InvalidInput;
    }
    /* Wait for sometime as specified by EMV before pulling RST High
     * As per EMV delay <= 42000 Clock cycles
     * as per PHY delay >= 1us */
    uint32_t temp =
        ((((uint32_t)10000u * context->interfaceConfig.clockToResetDelay) / context->interfaceConfig.smartCardClock) *
         100u) +
        1u;
    context->timeDelay(temp);
/* Pull reset HIGH Now to mark the end of Activation sequence */
#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT)
    emvsimBase->PCSR |= EMVSIM_PCSR_SRST_MASK;
#else
    ((GPIO_Type *)gpio_base[context->interfaceConfig.resetPort])->PSOR |= (1u << context->interfaceConfig.resetPin);
#endif
/* Configure TS character and ATR duration timers and enable receiver */
#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT)
    emvsimBase->CLKCFG &= ~(EMVSIM_CLKCFG_GPCNT0_CLK_SEL_MASK | EMVSIM_CLKCFG_GPCNT1_CLK_SEL_MASK);
    /* Down counter trigger, and clear any pending counter status flag */
    emvsimBase->TX_STATUS = EMVSIM_TX_STATUS_GPCNT1_TO_MASK | EMVSIM_TX_STATUS_GPCNT0_TO_MASK;
    /* Set counter value for TS detection delay */
    emvsimBase->GPCNT0_VAL = (SMARTCARD_INIT_DELAY_CLOCK_CYCLES + SMARTCARD_INIT_DELAY_CLOCK_CYCLES_ADJUSTMENT);
    /* Pre-load counter value for ATR duration delay */
    emvsimBase->GPCNT1_VAL = (SMARTCARD_EMV_ATR_DURATION_ETU + SMARTCARD_ATR_DURATION_ADJUSTMENT);
    /* Select the clock for GPCNT for both TS detection and early start of ATR duration counter */
    emvsimBase->CLKCFG |=
        (EMVSIM_CLKCFG_GPCNT0_CLK_SEL(kEMVSIM_GPCCardClock) | EMVSIM_CLKCFG_GPCNT1_CLK_SEL(kEMVSIM_GPCTxClock));
    /* Set receiver to ICM mode, Flush RX FIFO  */
    emvsimBase->CTRL |= (EMVSIM_CTRL_ICM_MASK | EMVSIM_CTRL_FLSH_RX_MASK);
    /* Enable counter interrupt for TS detection */
    emvsimBase->INT_MASK &= ~EMVSIM_INT_MASK_GPCNT0_IM_MASK;
    /* Clear any pending status flags */
    emvsimBase->RX_STATUS = 0xFFFFFFFFu;
    /* Enable receiver */
    emvsimBase->CTRL |= EMVSIM_CTRL_RCV_EN_MASK;
#else
    /* Enable external timer for TS detection time-out */
    smartcard_uart_TimerStart(context->interfaceConfig.tsTimerId,
                              (SMARTCARD_INIT_DELAY_CLOCK_CYCLES + SMARTCARD_INIT_DELAY_CLOCK_CYCLES_ADJUSTMENT) *
                                  (CLOCK_GetFreq(kCLOCK_BusClk) / context->interfaceConfig.smartCardClock));
#endif
    /* Here the card was activated */
    context->cardParams.active = true;

    return kStatus_SMARTCARD_Success;
}

status_t SMARTCARD_PHY_Deactivate(void *base, smartcard_context_t *context)
{
    if ((NULL == context))
    {
        return kStatus_SMARTCARD_InvalidInput;
    }

#if !(defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT))
    uint32_t gpio_base[] = GPIO_BASE_ADDRS;
#endif
/* Tell PHY to start Deactivation sequence by pulling CMD high and reset low */
#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT)
    ((EMVSIM_Type *)base)->PCSR |= EMVSIM_PCSR_SVCC_EN_MASK;
    ((EMVSIM_Type *)base)->PCSR &= ~EMVSIM_PCSR_VCCENP_MASK;
    ((EMVSIM_Type *)base)->PCSR &= ~EMVSIM_PCSR_SRST_MASK;
#else
    ((GPIO_Type *)gpio_base[context->interfaceConfig.controlPort])->PSOR |= (1u << context->interfaceConfig.controlPin);
    ((GPIO_Type *)gpio_base[context->interfaceConfig.resetPort])->PCOR |= (1u << context->interfaceConfig.resetPin);
#endif
    /* According EMV 4.3 specification deactivation sequence should be done within 100ms.
     * The period is measured from the time that RST is set to state L to the time that Vcc
     * reaches 0.4 V or less. */
    context->timeDelay(100 * 1000);
    /* Here the card was deactivated */
    context->cardParams.active = false;

    /* Fix for EMV Analog: Deactivate Clock after deactivation is completed. *
     * Otherwise the CLK line has a bump during EMVCo analog test, which cause a fail.
     */
    smartcard_phy_tda8035_InterfaceClockDisable(base, &context->interfaceConfig);

    return kStatus_SMARTCARD_Success;
}

status_t SMARTCARD_PHY_Control(void *base,
                               smartcard_context_t *context,
                               smartcard_interface_control_t control,
                               uint32_t param)
{
    if ((NULL == context))
    {
        return kStatus_SMARTCARD_InvalidInput;
    }
    status_t status = kStatus_SMARTCARD_Success;
#if !(defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT))
    uint32_t gpio_base[] = GPIO_BASE_ADDRS;
#endif

    switch (control)
    {
        case kSMARTCARD_InterfaceSetVcc:
            /* Set card parameter to VCC level set by caller */
            context->interfaceConfig.vcc = (smartcard_card_voltage_class_t)param;
            break;
        case kSMARTCARD_InterfaceSetClockToResetDelay:
            /* Set interface clock to Reset delay set by caller */
            context->interfaceConfig.clockToResetDelay = param;
            break;
        case kSMARTCARD_InterfaceReadStatus:
#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT) && (FSL_FEATURE_SOC_EMVSIM_COUNT)
            /* Expecting active low present detect */
            context->cardParams.present =
                ((emvsim_presence_detect_status_t)(uint32_t)((((EMVSIM_Type *)base)->PCSR & EMVSIM_PCSR_SPDP_MASK) >>
                                                             EMVSIM_PCSR_SPDP_SHIFT) == kEMVSIM_DetectPinIsLow);
#else
            if (((GPIO_Type *)gpio_base[context->interfaceConfig.controlPort])->PDIR &
                (1u << context->interfaceConfig.controlPin))
            {
                if (((GPIO_Type *)gpio_base[context->interfaceConfig.irqPort])->PDIR &
                    (1u << context->interfaceConfig.irqPin))
                { /* CMDVCC is high => session is inactive and INT is high => card is present */
                    context->cardParams.present = true;
                    context->cardParams.active  = false;
                    context->cardParams.faulty  = false;
                    context->cardParams.status  = SMARTCARD_TDA8035_STATUS_PRES;
                }
                else
                { /* CMDVCC is high => session is inactive and INT is low => card is absent */
                    context->cardParams.present = false;
                    context->cardParams.active  = false;
                    context->cardParams.faulty  = false;
                    context->cardParams.status  = 0u;
                }
            }
            else
            {
                if (((GPIO_Type *)gpio_base[context->interfaceConfig.irqPort])->PDIR &
                    (1u << context->interfaceConfig.irqPin))
                { /* CMDVCC is low => session is active and INT is high => card is present */
                    context->cardParams.present = true;
                    context->cardParams.active  = true;
                    context->cardParams.faulty  = false;
                    context->cardParams.status  = SMARTCARD_TDA8035_STATUS_PRES | SMARTCARD_TDA8035_STATUS_ACTIVE;
                }
                else
                {
                    /* CMDVCC is low => session is active and INT is high => card is absent/deactivated due to some
                     * fault
                     * A fault has been detected (card has been deactivated) but The cause of the deactivation is not
                     * yet known.
                     * Lets determine the cause of fault by pulling CMD high */
                    if (((GPIO_Type *)gpio_base[context->interfaceConfig.irqPort])->PDIR &
                        (1u << context->interfaceConfig.irqPin))
                    { /* The fault detected was not a card removal (card is still present) */
                        /* If INT follows CMDVCCN, the fault is due to a supply voltage drop, a VCC over-current
                         * detection or overheating. */
                        context->cardParams.present = true;
                        context->cardParams.active  = false;
                        context->cardParams.faulty  = true;
                        context->cardParams.status  = SMARTCARD_TDA8035_STATUS_PRES | SMARTCARD_TDA8035_STATUS_FAULTY |
                                                     SMARTCARD_TDA8035_STATUS_CARD_DEACTIVATED;
                    }
                    else
                    { /* The fault detected was the card removal
                       * Setting CMDVCCN allows checking if the deactivation is due to card removal.
                       * In this case the INT pin will stay low after CMDVCCN is high. */
                        context->cardParams.present = false;
                        context->cardParams.active  = false;
                        context->cardParams.faulty  = false;
                        context->cardParams.status =
                            SMARTCARD_TDA8035_STATUS_CARD_REMOVED | SMARTCARD_TDA8035_STATUS_CARD_DEACTIVATED;
                    }
                }
            }
#endif
            break;
        default:
            status = kStatus_SMARTCARD_InvalidInput;
            break;
    }

    return status;
}

void SMARTCARD_PHY_IRQHandler(void *base, smartcard_context_t *context)
{
    if ((NULL == context))
    {
        return;
    }
    /* Read interface/card status */
    (void)SMARTCARD_PHY_Control(base, context, kSMARTCARD_InterfaceReadStatus, 0u);
    /* Invoke callback if there is one */
    if (NULL != context->interfaceCallback)
    {
        context->interfaceCallback(context, context->interfaceCallbackParam);
    }
    SDK_ISR_EXIT_BARRIER;
}
