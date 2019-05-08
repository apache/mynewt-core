/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>
#include "syscfg/syscfg.h"
#include "mcu/da1469x_hal.h"
#include "mcu/da1469x_prail.h"
#include "mcu/da1469x_retreg.h"
#include "os/util.h"
#include "DA1469xAB.h"

#define POWER_CTRL_REG_SET(_field, _val)                                        \
    CRG_TOP->POWER_CTRL_REG =                                                   \
    (CRG_TOP->POWER_CTRL_REG & ~CRG_TOP_POWER_CTRL_REG_ ## _field ## _Msk) |    \
    ((_val) << CRG_TOP_POWER_CTRL_REG_ ## _field ## _Pos)

#if MYNEWT_VAL(MCU_DCDC_ENABLE)
/*
 * Need to store 5 registers:
 * DCDC_V18_REG, DCDC_V18P_REG, DCDC_VDD_RE, DCDC_V14_REG, DCDC_CTRL1_REG
 */
static struct da1469x_retreg g_mcu_dcdc_config[5];
#endif

static void
da1469x_prail_configure_3v0(void)
{
    /* 150mA max load active, 10mA max load sleep */
    /* XXX make rail configurable */

    POWER_CTRL_REG_SET(LDO_3V0_MODE, 3);                /* Automatic */
    POWER_CTRL_REG_SET(LDO_3V0_REF, 1);                 /* Bandgap */
    POWER_CTRL_REG_SET(V30_LEVEL, 0);                   /* 3.000 V */
    POWER_CTRL_REG_SET(LDO_3V0_RET_ENABLE_ACTIVE, 0);
    POWER_CTRL_REG_SET(LDO_3V0_RET_ENABLE_SLEEP, 1);
    POWER_CTRL_REG_SET(CLAMP_3V0_VBAT_ENABLE, 0);
}

static void
da1469x_prail_configure_1v8(void)
{
    /* 10mA max load active, 10mA max load sleep */
    /* XXX make rail configurable */

    POWER_CTRL_REG_SET(V18_LEVEL, 1);                   /* 1.800 V */
    POWER_CTRL_REG_SET(LDO_1V8_RET_ENABLE_ACTIVE, 1);
    POWER_CTRL_REG_SET(LDO_1V8_RET_ENABLE_SLEEP, 1);
    POWER_CTRL_REG_SET(LDO_1V8_ENABLE, 0);
}

static void
da1469x_prail_configure_1v8p(void)
{
    /* 50mA max load active, 10mA max load sleep */
    /* XXX make rail configurable */

    POWER_CTRL_REG_SET(LDO_1V8P_RET_ENABLE_ACTIVE, 0);
    POWER_CTRL_REG_SET(LDO_1V8P_RET_ENABLE_SLEEP, 1);
    POWER_CTRL_REG_SET(LDO_1V8P_ENABLE, 1);
}

static void
da1469x_prail_configure_1v2(void)
{
    /* 50mA max load active, 50mA max load sleep */
    /* XXX make rail configurable */

    /*
     * LDO_CORE_RET will be used instead of clamp when sleeping if VDD level
     * set for clamp is lower than set for sleep LDO.
     */

    POWER_CTRL_REG_SET(VDD_SLEEP_LEVEL, 0);             /* 0.750 V */
    POWER_CTRL_REG_SET(VDD_CLAMP_LEVEL, 15);            /* 0.706 V */
    POWER_CTRL_REG_SET(VDD_LEVEL, 3);                   /* 1.200 V */
    POWER_CTRL_REG_SET(LDO_CORE_RET_ENABLE_ACTIVE, 0);
    POWER_CTRL_REG_SET(LDO_CORE_RET_ENABLE_SLEEP, 1);
    POWER_CTRL_REG_SET(LDO_CORE_ENABLE, 1);
}

static void
da1469x_prail_configure_1v4(void)
{
    POWER_CTRL_REG_SET(V14_LEVEL, 4);                   /* 1.400 V */
    /*
     * XXX LDO_RADIO will be enabled when CMAC is initialized, but probably
     *     we'll need it for PLL as well.
     */
//    POWER_CTRL_REG_SET(LDO_RADIO_ENABLE, 1);
}

#if MYNEWT_VAL(MCU_DCDC_ENABLE)
void
da1469x_prail_dcdc_enable(void)
{
    DCDC->DCDC_V18_REG |= DCDC_DCDC_V18_REG_DCDC_V18_ENABLE_HV_Msk;
    DCDC->DCDC_V18_REG &= ~DCDC_DCDC_V18_REG_DCDC_V18_ENABLE_LV_Msk;

    DCDC->DCDC_V18P_REG |= DCDC_DCDC_V18P_REG_DCDC_V18P_ENABLE_HV_Msk;
    DCDC->DCDC_V18P_REG &= ~DCDC_DCDC_V18P_REG_DCDC_V18P_ENABLE_LV_Msk;

    DCDC->DCDC_VDD_REG |= DCDC_DCDC_VDD_REG_DCDC_VDD_ENABLE_HV_Msk;
    DCDC->DCDC_VDD_REG |= DCDC_DCDC_VDD_REG_DCDC_VDD_ENABLE_LV_Msk;

    DCDC->DCDC_V14_REG |= DCDC_DCDC_VDD_REG_DCDC_VDD_ENABLE_HV_Msk;
    DCDC->DCDC_V14_REG |= DCDC_DCDC_VDD_REG_DCDC_VDD_ENABLE_LV_Msk;

    da1469x_retreg_init(g_mcu_dcdc_config, ARRAY_SIZE(g_mcu_dcdc_config));
    da1469x_retreg_assign(&g_mcu_dcdc_config[0], &DCDC->DCDC_V18_REG);
    da1469x_retreg_assign(&g_mcu_dcdc_config[1], &DCDC->DCDC_V18P_REG);
    da1469x_retreg_assign(&g_mcu_dcdc_config[2], &DCDC->DCDC_VDD_REG);
    da1469x_retreg_assign(&g_mcu_dcdc_config[3], &DCDC->DCDC_V14_REG);
    da1469x_retreg_assign(&g_mcu_dcdc_config[4], &DCDC->DCDC_CTRL1_REG);

    /*
     * Enabling DCDC while VBAT is below 2.5 V renders system unstable even
     * if VBUS is available. Enable DCDC only if VBAT is above minimal value.
     */
    if (CRG_TOP->ANA_STATUS_REG & CRG_TOP_ANA_STATUS_REG_COMP_VBAT_HIGH_Msk) {
        DCDC->DCDC_CTRL1_REG |= DCDC_DCDC_CTRL1_REG_DCDC_ENABLE_Msk;
    }
}

void
da1469x_prail_dcdc_restore(void)
{
    /*
     * Enabling DCDC while VBAT is below 2.5 V renders system unstable even
     * if VBUS is available. Enable DCDC only if VBAT is above minimal value.
     */
    if (CRG_TOP->ANA_STATUS_REG & CRG_TOP_ANA_STATUS_REG_COMP_VBAT_HIGH_Msk) {
        da1469x_retreg_restore(g_mcu_dcdc_config, ARRAY_SIZE(g_mcu_dcdc_config));
        DCDC->DCDC_CTRL1_REG |= DCDC_DCDC_CTRL1_REG_DCDC_ENABLE_Msk;
    }
}
#endif

void
da1469x_prail_initialize(void)
{
    da1469x_prail_configure_3v0();
    da1469x_prail_configure_1v8();
    da1469x_prail_configure_1v8p();
    da1469x_prail_configure_1v2();
    da1469x_prail_configure_1v4();
}
