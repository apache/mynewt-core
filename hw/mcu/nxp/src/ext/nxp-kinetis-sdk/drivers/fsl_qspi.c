/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2019 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_qspi.h"

/* Component ID definition, used by tools. */
#ifndef FSL_COMPONENT_ID
#define FSL_COMPONENT_ID "platform.drivers.qspi"
#endif

/*******************************************************************************
 * Definitations
 ******************************************************************************/
enum _qspi_transfer_state
{
    kQSPI_TxBusy = 0x0U, /*!< QSPI is busy */
    kQSPI_TxIdle,        /*!< Transfer is done. */
    kQSPI_TxError        /*!< Transfer error occurred. */
};

#define QSPI_AHB_BUFFER_REG(base, index) (((volatile uint32_t *)&((base)->BUF0CR))[(index)])

#if (!defined(FSL_FEATURE_QSPI_HAS_NO_SOCCR_REG)) || !FSL_FEATURE_QSPI_HAS_NO_SOCCR_REG
#ifndef QuadSPI_SOCCR_DQS_LOOPBACK_EN_MASK
#define QuadSPI_SOCCR_DQS_LOOPBACK_EN_MASK (0x100U)
#endif

#ifndef QuadSPI_SOCCR_DQS_LOOPBACK_FROM_PAD_MASK
#define QuadSPI_SOCCR_DQS_LOOPBACK_FROM_PAD_MASK (0x200U)
#endif

#ifndef QuadSPI_SOCCR_DQS_PHASE_SEL_MASK
#define QuadSPI_SOCCR_DQS_PHASE_SEL_MASK  (0xC00U)
#define QuadSPI_SOCCR_DQS_PHASE_SEL_SHIFT (10U)
#define QuadSPI_SOCCR_DQS_PHASE_SEL(x) \
    (((uint32_t)(((uint32_t)(x)) << QuadSPI_SOCCR_DQS_PHASE_SEL_SHIFT)) & QuadSPI_SOCCR_DQS_PHASE_SEL_MASK)
#endif

#ifndef QuadSPI_SOCCR_DQS_INV_EN_MASK
#define QuadSPI_SOCCR_DQS_INV_EN_MASK  (0x1000U)
#define QuadSPI_SOCCR_DQS_INV_EN_SHIFT (12U)
#define QuadSPI_SOCCR_DQS_INV_EN(x) \
    (((uint32_t)(((uint32_t)(x)) << QuadSPI_SOCCR_DQS_INV_EN_SHIFT)) & QuadSPI_SOCCR_DQS_INV_EN_MASK)
#endif

#ifndef QuadSPI_SOCCR_DQS_IFA_DELAY_CHAIN_SEL_MASK
#define QuadSPI_SOCCR_DQS_IFA_DELAY_CHAIN_SEL_MASK  (0x7F0000U)
#define QuadSPI_SOCCR_DQS_IFA_DELAY_CHAIN_SEL_SHIFT (16U)
#define QuadSPI_SOCCR_DQS_IFA_DELAY_CHAIN_SEL(x)                                    \
    (((uint32_t)(((uint32_t)(x)) << QuadSPI_SOCCR_DQS_IFA_DELAY_CHAIN_SEL_SHIFT)) & \
     QuadSPI_SOCCR_DQS_IFA_DELAY_CHAIN_SEL_MASK)
#endif
#endif /* FSL_FEATURE_QSPI_HAS_NO_SOCCR_REG */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
/* Base pointer array */
static QuadSPI_Type *const s_qspiBases[] = QuadSPI_BASE_PTRS;
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
/* Clock name array */
static const clock_ip_name_t s_qspiClock[] = QSPI_CLOCKS;
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */

/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * brief Get the instance number for QSPI.
 *
 * param base QSPI base pointer.
 */
uint32_t QSPI_GetInstance(QuadSPI_Type *base)
{
    uint32_t instance;

    /* Find the instance index from base address mappings. */
    for (instance = 0; instance < ARRAY_SIZE(s_qspiBases); instance++)
    {
        if (s_qspiBases[instance] == base)
        {
            break;
        }
    }

    assert(instance < ARRAY_SIZE(s_qspiBases));

    return instance;
}

/*!
 * brief Initializes the QSPI module and internal state.
 *
 * This function enables the clock for QSPI and also configures the QSPI with the
 * input configure parameters. Users should call this function before any QSPI operations.
 *
 * param base Pointer to QuadSPI Type.
 * param config QSPI configure structure.
 * param srcClock_Hz QSPI source clock frequency in Hz.
 */
void QSPI_Init(QuadSPI_Type *base, qspi_config_t *config, uint32_t srcClock_Hz)
{
    uint32_t i   = 0;
    uint32_t val = 0;

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    /* Enable QSPI clock */
    CLOCK_EnableClock(s_qspiClock[QSPI_GetInstance(base)]);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */

    /* Do software reset to QSPI module */
    QSPI_SoftwareReset(base);

    /* Clear the FIFO region */
    QSPI_ClearFifo(base, (uint32_t)kQSPI_AllFifo);

    /* Configure QSPI */
    QSPI_Enable(base, false);

#if !defined(FSL_FEATURE_QSPI_CLOCK_CONTROL_EXTERNAL) || (!FSL_FEATURE_QSPI_CLOCK_CONTROL_EXTERNAL)
    /* Set qspi clock source */
    base->SOCCR = config->clockSource;

    /* Read MCR value, mask SCLKCFG field */
    val = base->MCR;
    val &= ~QuadSPI_MCR_SCLKCFG_MASK;

    /* To avoid the configured baudrate exceeds the expected baudrate value, which may possibly put the
    QSPI work under unsupported frequency, set the divider higher when there is reminder, use ceiling
    operation, ceiling(a/b) = (a-1)/b + 1. */
    val |= QuadSPI_MCR_SCLKCFG((srcClock_Hz - 1U) / config->baudRate);
    base->MCR = val;
#endif /* FSL_FEATURE_QSPI_CLOCK_CONTROL_EXTERNAL */

    /* Set AHB buffer size and buffer master */
    for (i = 0; i < (uint32_t)FSL_FEATURE_QSPI_AHB_BUFFER_COUNT; i++)
    {
        val = QuadSPI_BUF0CR_MSTRID(config->AHBbufferMaster[i]) | QuadSPI_BUF0CR_ADATSZ(config->AHBbufferSize[i] / 8U);
        QSPI_AHB_BUFFER_REG(base, i) = val;
    }
    if (config->enableAHBbuffer3AllMaster)
    {
        base->BUF3CR |= QuadSPI_BUF3CR_ALLMST_MASK;
    }
    else
    {
        base->BUF3CR &= ~QuadSPI_BUF3CR_ALLMST_MASK;
    }

    /* Set watermark */
    base->RBCT &= ~QuadSPI_RBCT_WMRK_MASK;
    base->RBCT |= QuadSPI_RBCT_WMRK((uint32_t)config->rxWatermark - 1U);

#if !defined(FSL_FEATURE_QSPI_HAS_NO_TXDMA) || (!FSL_FEATURE_QSPI_HAS_NO_TXDMA)
    base->TBCT &= ~QuadSPI_TBCT_WMRK_MASK;
    base->TBCT |= QuadSPI_TBCT_WMRK((uint32_t)config->txWatermark - 1U);
#endif /* FSL_FEATURE_QSPI_HAS_NO_TXDMA */

    /* Enable QSPI module */
    if (config->enableQspi)
    {
        QSPI_Enable(base, true);
    }
}

/*!
 * brief Gets default settings for QSPI.
 *
 * param config QSPI configuration structure.
 */
void QSPI_GetDefaultQspiConfig(qspi_config_t *config)
{
    /* Initializes the configure structure to zero. */
    (void)memset(config, 0, sizeof(*config));

    config->clockSource               = 2U;
    config->baudRate                  = 24000000U;
    config->AHBbufferMaster[0]        = 0xE;
    config->AHBbufferMaster[1]        = 0xE;
    config->AHBbufferMaster[2]        = 0xE;
    config->enableAHBbuffer3AllMaster = true;
    config->txWatermark               = 8U;
    config->rxWatermark               = 8U;
    config->enableQspi                = true;
}

/*!
 * brief Deinitializes the QSPI module.
 *
 * Clears the QSPI state and  QSPI module registers.
 * param base Pointer to QuadSPI Type.
 */
void QSPI_Deinit(QuadSPI_Type *base)
{
    QSPI_Enable(base, false);
#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    CLOCK_DisableClock(s_qspiClock[QSPI_GetInstance(base)]);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
}

/*!
 * brief Configures the serial flash parameter.
 *
 * This function configures the serial flash relevant parameters, such as the size, command, and so on.
 * The flash configuration value cannot have a default value. The user needs to configure it according to the
 * QSPI features.
 *
 * param base Pointer to QuadSPI Type.
 * param config Flash configuration parameters.
 */
void QSPI_SetFlashConfig(QuadSPI_Type *base, qspi_flash_config_t *config)
{
    uint32_t address = FSL_FEATURE_QSPI_AMBA_BASE + config->flashA1Size;
    uint32_t val     = 0;
    uint32_t i       = 0;

    /* Disable module */
    QSPI_Enable(base, false);

    /* Config the serial flash size */
    base->SFA1AD = address;
    address += config->flashA2Size;
    base->SFA2AD = address;
#if defined(FSL_FEATURE_QSPI_SUPPORT_PARALLEL_MODE) && (FSL_FEATURE_QSPI_SUPPORT_PARALLEL_MODE)
    address += config->flashB1Size;
    base->SFB1AD = address;
    address += config->flashB2Size;
    base->SFB2AD = address;
#endif /* FSL_FEATURE_QSPI_SUPPORT_PARALLEL_MODE */

#if !defined(FSL_FEATURE_QSPI_HAS_NO_SFACR) || (!FSL_FEATURE_QSPI_HAS_NO_SFACR)
    /* Set Word Addressable feature */
    val         = QuadSPI_SFACR_WA(config->enableWordAddress) | QuadSPI_SFACR_CAS(config->cloumnspace);
    base->SFACR = val;
#endif /* FSL_FEATURE_QSPI_HAS_NO_SFACR */

    /* Config look up table */
    base->LUTKEY = 0x5AF05AF0U;
    base->LCKCR  = 0x2U;
    for (i = 0; i < (uint32_t)FSL_FEATURE_QSPI_LUT_DEPTH; i++)
    {
        base->LUT[i] = config->lookuptable[i];
    }
    base->LUTKEY = 0x5AF05AF0U;
    base->LCKCR  = 0x1U;

#if !defined(FSL_FEATURE_QSPI_HAS_NO_TDH) || (!FSL_FEATURE_QSPI_HAS_NO_TDH)
    /* Config flash timing */
    val = QuadSPI_FLSHCR_TCSS(config->CSHoldTime) | QuadSPI_FLSHCR_TDH(config->dataHoldTime) |
          QuadSPI_FLSHCR_TCSH(config->CSSetupTime);
#else
    val = QuadSPI_FLSHCR_TCSS(config->CSHoldTime) | QuadSPI_FLSHCR_TCSH(config->CSSetupTime);
#endif /* FSL_FEATURE_QSPI_HAS_NO_TDH */
    base->FLSHCR = val;

    /* Set flash endianness */
    base->MCR &= ~QuadSPI_MCR_END_CFG_MASK;
    base->MCR |= QuadSPI_MCR_END_CFG(config->endian);

    /* Enable QSPI again */
    QSPI_Enable(base, true);
}

#if (!defined(FSL_FEATURE_QSPI_HAS_NO_SOCCR_REG)) || !FSL_FEATURE_QSPI_HAS_NO_SOCCR_REG
/*!
 * @brief Configures the serial flash DQS parameter.
 *
 * This function configures the serial flash DQS relevant parameters, such as the delay chain tap number, .
 * DQS shift phase, whether need to inverse and the rxc sample clock selection.
 *
 * @param base Pointer to QuadSPI Type.
 * @param config Dqs configuration parameters.
 */
void QSPI_SetDqsConfig(QuadSPI_Type *base, qspi_dqs_config_t *config)
{
    uint32_t soccrVal;
    uint32_t mcrVal;

    /* Disable module */
    QSPI_Enable(base, false);

    mcrVal = base->MCR;

    mcrVal &= ~(QuadSPI_MCR_DQS_EN_MASK | QuadSPI_MCR_DQS_LAT_EN_MASK);
    /* Enable DQS. */
    mcrVal |= QuadSPI_MCR_DQS_EN_MASK;

    /* Configure DQS phase, inverse and loopback atrribute */
    soccrVal = base->SOCCR;
    soccrVal &=
        ~(QuadSPI_SOCCR_DQS_LOOPBACK_EN_MASK | QuadSPI_SOCCR_DQS_LOOPBACK_FROM_PAD_MASK |
          QuadSPI_SOCCR_DQS_PHASE_SEL_MASK | QuadSPI_SOCCR_DQS_INV_EN_MASK | QuadSPI_SOCCR_DQS_IFA_DELAY_CHAIN_SEL_MASK
#if defined(QuadSPI_SOCCR_DQS_IFB_DELAY_CHAIN_SEL_MASK)
          | QuadSPI_SOCCR_DQS_IFB_DELAY_CHAIN_SEL_MASK
#endif
        );
    soccrVal |= QuadSPI_SOCCR_DQS_PHASE_SEL(config->shift);

    switch (config->rxSampleClock)
    {
        case kQSPI_ReadSampleClkInternalLoopback:
            soccrVal |= QuadSPI_SOCCR_DQS_LOOPBACK_EN_MASK;
            break;
        case kQSPI_ReadSampleClkExternalInputFromDqsPad:
            mcrVal |= QuadSPI_MCR_DQS_LAT_EN_MASK;
            break;
        case kQSPI_ReadSampleClkLoopbackFromDqsPad:
            soccrVal |= QuadSPI_SOCCR_DQS_LOOPBACK_FROM_PAD_MASK;
            break;
        default:
            assert(false);
            break;
    }

    soccrVal |= (QuadSPI_SOCCR_DQS_INV_EN(config->enableDQSClkInverse) |
                 QuadSPI_SOCCR_DQS_IFA_DELAY_CHAIN_SEL(config->portADelayTapNum)
#if defined(QuadSPI_SOCCR_DQS_IFB_DELAY_CHAIN_SEL_MASK)
                 | QuadSPI_SOCCR_DQS_IFB_DELAY_CHAIN_SEL(config->portBDelayTapNum)
#endif
    );

    base->MCR   = mcrVal;
    base->SOCCR = soccrVal;

    /* Enable QSPI again */
    QSPI_Enable(base, true);
}
#endif /* FSL_FEATURE_QSPI_HAS_NO_SOCCR_REG */

/*!
 * brief Software reset for the QSPI logic.
 *
 * This function sets the software reset flags for both AHB and buffer domain and
 * resets both AHB buffer and also IP FIFOs.
 *
 * param base Pointer to QuadSPI Type.
 */
void QSPI_SoftwareReset(QuadSPI_Type *base)
{
    uint32_t i = 0;

    /* Reset AHB domain and buffer domian */
    base->MCR |= (QuadSPI_MCR_SWRSTHD_MASK | QuadSPI_MCR_SWRSTSD_MASK);

    /* Wait several time for the reset to finish, this method came from IC team */
    for (i = 0; i < 100U; i++)
    {
        __NOP();
    }

    /* Disable QSPI module */
    QSPI_Enable(base, false);

    /* Clear the reset flags */
    base->MCR &= ~(QuadSPI_MCR_SWRSTHD_MASK | QuadSPI_MCR_SWRSTSD_MASK);

    /* Enable QSPI module */
    QSPI_Enable(base, true);
}

/*!
 * brief Gets the Rx data register address used for DMA operation.
 *
 * This function returns the Rx data register address or Rx buffer address
 * according to the Rx read area settings.
 *
 * param base Pointer to QuadSPI Type.
 * return QSPI Rx data register address.
 */
uint32_t QSPI_GetRxDataRegisterAddress(QuadSPI_Type *base)
{
    /* From RDBR */
    if (0U != (base->RBCT & QuadSPI_RBCT_RXBRD_MASK))
    {
        return (uint32_t)(&(base->RBDR[0]));
    }
    else
    {
        /* From ARDB */
        return FSL_FEATURE_QSPI_ARDB_BASE;
    }
}

/*! brief Executes IP commands located in LUT table.
 *
 * param base Pointer to QuadSPI Type.
 * param index IP command located in which LUT table index.
 */
void QSPI_ExecuteIPCommand(QuadSPI_Type *base, uint32_t index)
{
    while (0U != (QSPI_GetStatusFlags(base) & ((uint32_t)kQSPI_Busy | (uint32_t)kQSPI_IPAccess)))
    {
    }
    QSPI_ClearCommandSequence(base, kQSPI_IPSeq);

    /* Write the seqid bit */
    base->IPCR = ((base->IPCR & (~QuadSPI_IPCR_SEQID_MASK)) | QuadSPI_IPCR_SEQID(index / 4U));
}

/*! brief Executes AHB commands located in LUT table.
 *
 * param base Pointer to QuadSPI Type.
 * param index AHB command located in which LUT table index.
 */
void QSPI_ExecuteAHBCommand(QuadSPI_Type *base, uint32_t index)
{
    while (0U != (QSPI_GetStatusFlags(base) & ((uint32_t)kQSPI_Busy | (uint32_t)kQSPI_AHBAccess)))
    {
    }
    QSPI_ClearCommandSequence(base, kQSPI_BufferSeq);
    base->BFGENCR = ((base->BFGENCR & (~QuadSPI_BFGENCR_SEQID_MASK)) | QuadSPI_BFGENCR_SEQID(index / 4U));
}

/*! brief Updates the LUT table.
 *
 * param base Pointer to QuadSPI Type.
 * param index Which LUT index needs to be located. It should be an integer divided by 4.
 * param cmd Command sequence array.
 */
void QSPI_UpdateLUT(QuadSPI_Type *base, uint32_t index, uint32_t *cmd)
{
    uint8_t i = 0;

    /* Unlock the LUT */
    base->LUTKEY = 0x5AF05AF0U;
    base->LCKCR  = 0x2U;

    /* Write data into LUT */
    for (i = 0; i < 4U; i++)
    {
        base->LUT[index + i] = *cmd;
        cmd++;
    }

    /* Lcok LUT again */
    base->LUTKEY = 0x5AF05AF0U;
    base->LCKCR  = 0x1U;
}

#if defined(FSL_FEATURE_QSPI_SOCCR_HAS_CLR_LPCAC) && (FSL_FEATURE_QSPI_SOCCR_HAS_CLR_LPCAC)

/*! brief Clears the QSPI cache.
 *
 * param base Pointer to QuadSPI Type.
 */
void QSPI_ClearCache(QuadSPI_Type *base)
{
    uint32_t soccrVal;

    soccrVal = base->SOCCR;
    /* Write 1 to clear cache. */
    base->SOCCR = (soccrVal | QuadSPI_SOCCR_CLR_LPCAC_MASK);

    /* Write 0 to after cache is cleared. */
    base->SOCCR = (soccrVal & (~QuadSPI_SOCCR_CLR_LPCAC_MASK));
}
#endif

/*! brief Set the RX buffer readout area.
 *
 * This function can set the RX buffer readout, from AHB bus or IP Bus.
 * param base QSPI base address.
 * param area QSPI Rx buffer readout area. AHB bus buffer or IP bus buffer.
 */
void QSPI_SetReadDataArea(QuadSPI_Type *base, qspi_read_area_t area)
{
    base->RBCT &= ~QuadSPI_RBCT_RXBRD_MASK;
    base->RBCT |= QuadSPI_RBCT_RXBRD(area);
}

/*!
 * brief Receives data from data FIFO.
 *
 * param base QSPI base pointer
 * return The data in the FIFO.
 */
uint32_t QSPI_ReadData(QuadSPI_Type *base)
{
    if (0U != (base->RBCT & QuadSPI_RBCT_RXBRD_MASK))
    {
        return base->RBDR[0];
    }
    else
    {
        /* Data from ARDB. */
        return *((uint32_t *)FSL_FEATURE_QSPI_ARDB_BASE);
    }
}

/*!
 * brief Sends a buffer of data bytes using a  blocking method.
 * note This function blocks via polling until all bytes have been sent.
 * param base QSPI base pointer
 * param buffer The data bytes to send
 * param size The number of data bytes to send
 */
void QSPI_WriteBlocking(QuadSPI_Type *base, uint32_t *buffer, size_t size)
{
    assert(size >= 16U);

    uint32_t i = 0;

    for (i = 0; i < size / 4U; i++)
    {
        /* Check if the buffer is full */
        while (0U != (QSPI_GetStatusFlags(base) & (uint32_t)kQSPI_TxBufferFull))
        {
        }
        base->TBDR = *buffer++;
    }
}

/*!
 * brief Receives a buffer of data bytes using a blocking method.
 * note This function blocks via polling until all bytes have been sent. Users shall notice that
 * this receive size shall not bigger than 64 bytes. As this interface is used to read flash status registers.
 * For flash contents read, please use AHB bus read, this is much more efficiency.
 *
 * param base QSPI base pointer
 * param buffer The data bytes to send
 * param size The number of data bytes to receive
 */
void QSPI_ReadBlocking(QuadSPI_Type *base, uint32_t *buffer, size_t size)
{
    uint32_t i     = 0;
    uint32_t j     = 0;
    uint32_t temp  = 0;
    uint32_t level = (base->RBCT & QuadSPI_RBCT_WMRK_MASK) + 1U;

    while (i < size / 4U)
    {
        /* Check if there is data */
        if ((size / 4U - i) < level)
        {
            do
            {
                temp = (base->RBSR & QuadSPI_RBSR_RDBFL_MASK) >> QuadSPI_RBSR_RDBFL_SHIFT;
            } while (0U == temp);
        }
        else
        {
            while ((QSPI_GetStatusFlags(base) & (uint32_t)kQSPI_RxWatermark) == 0U)
            {
            }
        }

        level = (level < (size / 4U - i)) ? level : (size / 4U - i);

        /* Data from RBDR */
        if (0U != (base->RBCT & QuadSPI_RBCT_RXBRD_MASK))
        {
            for (j = 0; j < level; j++)
            {
                buffer[i + j] = base->RBDR[j];
            }
        }
        else
        {
            /* Data from ARDB. */
            for (j = 0; j < level; j++)
            {
                buffer[i + j] = ((uint32_t *)FSL_FEATURE_QSPI_ARDB_BASE)[j];
            }
        }
        i += level;

        /* Clear the Buffer */
        QSPI_ClearErrorFlag(base, (uint32_t)kQSPI_RxBufferDrain);
    }
}
