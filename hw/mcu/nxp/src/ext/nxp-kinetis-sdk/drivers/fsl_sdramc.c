/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2019 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_sdramc.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* Component ID definition, used by tools. */
#ifndef FSL_COMPONENT_ID
#define FSL_COMPONENT_ID "platform.drivers.sdramc"
#endif

/*! @brief Define macros for SDRAM driver. */
#define SDRAMC_ONEMILLSEC_NANOSECONDS (1000000U)
#define SDRAMC_ONESECOND_MILLISECONDS (1000U)

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/*!
 * @brief Get instance number for SDRAMC module.
 *
 * @param base SDRAMC peripheral base address
 */
static uint32_t SDRAMC_GetInstance(SDRAM_Type *base);

/*******************************************************************************
 * Variables
 ******************************************************************************/

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
/*! @brief Pointers to SDRAMC clocks for each instance. */
static const clock_ip_name_t s_sdramClock[FSL_FEATURE_SOC_SDRAM_COUNT] = SDRAM_CLOCKS;
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */

/*! @brief Pointers to SDRAMC bases for each instance. */
static SDRAM_Type *const s_sdramcBases[] = SDRAM_BASE_PTRS;
/*******************************************************************************
 * Code
 ******************************************************************************/

static uint32_t SDRAMC_GetInstance(SDRAM_Type *base)
{
    uint32_t instance;

    /* Find the instance index from base address mappings. */
    for (instance = 0; instance < ARRAY_SIZE(s_sdramcBases); instance++)
    {
        if (s_sdramcBases[instance] == base)
        {
            break;
        }
    }

    assert(instance < ARRAY_SIZE(s_sdramcBases));

    return instance;
}

/*!
 * brief Initializes the SDRAM controller.
 * This function ungates the SDRAM controller clock and initializes the SDRAM controller.
 * This function must be called before calling any other SDRAM controller driver functions.
 * Example
   code
    sdramc_refresh_config_t refreshConfig;
    sdramc_blockctl_config_t blockConfig;
    sdramc_config_t config;

    refreshConfig.refreshTime  = kSDRAM_RefreshThreeClocks;
    refreshConfig.sdramRefreshRow = 15625;
    refreshConfig.busClock = 60000000;

    blockConfig.block = kSDRAMC_Block0;
    blockConfig.portSize = kSDRAMC_PortSize16Bit;
    blockConfig.location = kSDRAMC_Commandbit19;
    blockConfig.latency = kSDRAMC_RefreshThreeClocks;
    blockConfig.address = SDRAM_START_ADDRESS;
    blockConfig.addressMask = 0x7c0000;

    config.refreshConfig = &refreshConfig,
    config.blockConfig = &blockConfig,
    config.totalBlocks = 1;

    SDRAMC_Init(SDRAM, &config);
   endcode
 *
 * param base SDRAM controller peripheral base address.
 * param configure The SDRAM configuration structure pointer.
 */
void SDRAMC_Init(SDRAM_Type *base, sdramc_config_t *configure)
{
    assert(configure);
    assert(configure->refreshConfig);
    assert(configure->blockConfig);
    assert(configure->refreshConfig->busClock_Hz);

    sdramc_blockctl_config_t *bctlConfig   = configure->blockConfig;
    sdramc_refresh_config_t *refreshConfig = configure->refreshConfig;
    uint32_t count;
    uint32_t index;

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    /* Un-gate sdram controller clock. */
    CLOCK_EnableClock(s_sdramClock[SDRAMC_GetInstance(base)]);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */

    /* Initialize sdram Auto refresh timing. */
    count      = refreshConfig->sdramRefreshRow * (refreshConfig->busClock_Hz / SDRAMC_ONESECOND_MILLISECONDS);
    count      = (count / SDRAMC_ONEMILLSEC_NANOSECONDS) / 16U - 1U;
    base->CTRL = SDRAM_CTRL_RC(count) | SDRAM_CTRL_RTIM(refreshConfig->refreshTime);

    for (index = 0; index < configure->numBlockConfig; index++)
    {
        /* Set the sdram block control. */
        base->BLOCK[index].AC = SDRAM_AC_PS(bctlConfig->portSize) | SDRAM_AC_CASL(bctlConfig->latency) |
                                SDRAM_AC_CBM(bctlConfig->location) | (bctlConfig->address & SDRAM_AC_BA_MASK);

        base->BLOCK[index].CM = (bctlConfig->addressMask & SDRAM_CM_BAM_MASK) | SDRAM_CM_V_MASK;

        /* Increases to the next sdram block. */
        bctlConfig++;
    }
}

/*!
 * brief Deinitializes the SDRAM controller module and gates the clock.
 * This function gates the SDRAM controller clock. As a result, the SDRAM
 * controller module doesn't work after calling this function.
 *
 * param base SDRAM controller peripheral base address.
 */
void SDRAMC_Deinit(SDRAM_Type *base)
{
    /* Set the SDRAMC invalid, do not decode DRAM accesses. */
    SDRAMC_EnableOperateValid(base, kSDRAMC_Block0, false);
    SDRAMC_EnableOperateValid(base, kSDRAMC_Block1, false);

#if !(defined(FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL) && FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL)
    /* Disable SDRAM clock. */
    CLOCK_DisableClock(s_sdramClock[SDRAMC_GetInstance(base)]);
#endif /* FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL */
}

/*!
 * brief Sends the SDRAM command.
 * This function sends commands to SDRAM. The commands are precharge command, initialization MRS command,
 * auto-refresh enable/disable command, and self-refresh enter/exit commands.
 * Note that the self-refresh enter/exit commands are all blocks setting and "block"
 * is ignored. Ensure to set the correct "block" when send other commands.
 *
 * param base SDRAM controller peripheral base address.
 * param block The block selection.
 * param command The SDRAM command, see "sdramc_command_t".
 *        kSDRAMC_ImrsCommand -  Initialize MRS command   \n
 *        kSDRAMC_PrechargeCommand  - Initialize precharge command   \n
 *        kSDRAMC_SelfrefreshEnterCommand - Enter self-refresh command \n
 *        kSDRAMC_SelfrefreshExitCommand  -  Exit self-refresh command \n
 *        kSDRAMC_AutoRefreshEnableCommand  - Enable auto refresh command \n
 *        kSDRAMC_AutoRefreshDisableCommand  - Disable auto refresh command
 */
void SDRAMC_SendCommand(SDRAM_Type *base, sdramc_block_selection_t block, sdramc_command_t command)
{
    switch (command)
    {
        /* Initiate mrs command. */
        case kSDRAMC_ImrsCommand:
            base->BLOCK[block].AC |= SDRAM_AC_IMRS_MASK;
            break;
        /* Initiate precharge command. */
        case kSDRAMC_PrechargeCommand:
            base->BLOCK[block].AC |= SDRAM_AC_IP_MASK;
            break;
        /* Enable Auto refresh command. */
        case kSDRAMC_AutoRefreshEnableCommand:
            base->BLOCK[block].AC |= SDRAM_AC_RE_MASK;
            break;
        /* Disable Auto refresh command. */
        case kSDRAMC_AutoRefreshDisableCommand:
            base->BLOCK[block].AC &= ~SDRAM_AC_RE_MASK;
            break;
        /* Enter self-refresh command. */
        case kSDRAMC_SelfrefreshEnterCommand:
            base->CTRL |= SDRAM_CTRL_IS_MASK;
            break;
        /* Exit self-refresh command. */
        case kSDRAMC_SelfrefreshExitCommand:
            base->CTRL &= ~(uint16_t)SDRAM_CTRL_IS_MASK;
            break;
        default:
            assert(false);
            break;
    }
}
