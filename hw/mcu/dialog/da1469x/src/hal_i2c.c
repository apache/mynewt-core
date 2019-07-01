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

#include <stdint.h>
#include <stdbool.h>
#include "syscfg/syscfg.h"
#include "os/os_time.h"
#include "mcu/da1469x_pd.h"
#include "mcu/da1469x_hal.h"
#include "mcu/mcu.h"
#include "hal/hal_i2c.h"
#include "hal/hal_gpio.h"
#include "DA1469xAB.h"

#define DA1469X_HAL_I2C_MAX (2)

struct da1469x_hal_i2c {
    I2C_Type *regs;
    uint8_t scl_func;
    uint8_t sda_func;
    IRQn_Type irqn;
};

#if MYNEWT_VAL(I2C_0)
static const struct da1469x_hal_i2c hal_i2c0 = {
    .regs = (void *)I2C_BASE,
    .scl_func = MCU_GPIO_FUNC_I2C_SCL,
    .sda_func = MCU_GPIO_FUNC_I2C_SDA,
    .irqn = I2C_IRQn
};
#endif
#if MYNEWT_VAL(I2C_1)
static const struct da1469x_hal_i2c hal_i2c1 = {
    .regs = (void *)I2C2_BASE,
    .scl_func = MCU_GPIO_FUNC_I2C2_SCL,
    .sda_func = MCU_GPIO_FUNC_I2C2_SDA,
    .irqn = I2C2_IRQn
};
#endif

const static struct da1469x_hal_i2c *da1469x_hal_i2cs[DA1469X_HAL_I2C_MAX] = {
#if MYNEWT_VAL(I2C_0)
    &hal_i2c0,
#else
    NULL,
#endif
#if MYNEWT_VAL(I2C_1)
    &hal_i2c1
#else
    NULL
#endif
};

static const struct da1469x_hal_i2c *
hal_i2c_resolve(uint8_t i2c_num)
{
    if (i2c_num >= DA1469X_HAL_I2C_MAX) {
        return NULL;
    }

    return da1469x_hal_i2cs[i2c_num];
}

int
hal_i2c_enable(uint8_t i2c_num)
{
    const struct da1469x_hal_i2c *i2c;

    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
       return HAL_I2C_ERR_INVAL;
    }

    /* This will also clear I2C_ABORT and I2C_TX_CMD_BLOCK */
    i2c->regs->I2C_ENABLE_REG = (1 << I2C_I2C_ENABLE_REG_I2C_EN_Pos);

    return 0;
}

int
hal_i2c_disable(uint8_t i2c_num)
{
    const struct da1469x_hal_i2c *i2c;

    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
       return HAL_I2C_ERR_INVAL;
    }

    i2c->regs->I2C_ENABLE_REG &= ~(1 << I2C_I2C_ENABLE_REG_I2C_EN_Pos);

    return 0;
}

static void
i2c_init_hw(const struct da1469x_hal_i2c *i2c, int pin_scl, int pin_sda)
{
    uint32_t i2c_con_reg;

    da1469x_pd_acquire(MCU_PD_DOMAIN_COM);

    /* Configure SCL, SDA.*/
    mcu_gpio_set_pin_function(pin_scl, MCU_GPIO_MODE_OUTPUT, i2c->scl_func);
    mcu_gpio_set_pin_function(pin_sda, MCU_GPIO_MODE_OUTPUT, i2c->sda_func);

    if (i2c == da1469x_hal_i2cs[0]) {
        CRG_COM->RESET_CLK_COM_REG = CRG_COM_RESET_CLK_COM_REG_I2C_CLK_SEL_Msk;
        CRG_COM->SET_CLK_COM_REG = CRG_COM_RESET_CLK_COM_REG_I2C_ENABLE_Msk;
    } else {
        CRG_COM->RESET_CLK_COM_REG = CRG_COM_RESET_CLK_COM_REG_I2C2_CLK_SEL_Msk;
        CRG_COM->SET_CLK_COM_REG = CRG_COM_RESET_CLK_COM_REG_I2C2_ENABLE_Msk;
    }

    /* Abort ongoing transactions and disable I2C */
    i2c->regs->I2C_ENABLE_REG |= (1 << I2C_I2C_ENABLE_REG_I2C_ABORT_Pos);
    i2c->regs->I2C_ENABLE_REG &= ~(1 << I2C_I2C_ENABLE_REG_I2C_EN_Pos);
    while (i2c->regs->I2C_ENABLE_STATUS_REG & I2C_I2C_ENABLE_STATUS_REG_IC_EN_Msk);
    /* TODO Maybe should wait here some time */

    /* Configure I2C_CON_REG, first configure to master */
    i2c_con_reg = (1 << I2C_I2C_CON_REG_I2C_MASTER_MODE_Pos);

    /* Use 7-bit addressing */
    i2c_con_reg &= ~(1 << I2C_I2C_CON_REG_I2C_10BITADDR_MASTER_Pos);

    /* Set 100 as default speed */
    i2c_con_reg |= (1 << I2C_I2C_CON_REG_I2C_SPEED_Pos);

    i2c->regs->I2C_CON_REG = i2c_con_reg;

    i2c->regs->I2C_INTR_MASK_REG = 0x0000;

    NVIC_EnableIRQ(i2c->irqn);
}

static int
i2c_config(const struct da1469x_hal_i2c *i2c, uint32_t frequency)
{
    uint32_t i2c_con_reg;

    /* Configure I2C_CON_REG */
    i2c_con_reg = i2c->regs->I2C_CON_REG;

    /* Clear speed register */
    i2c_con_reg &= ~I2C_I2C_CON_REG_I2C_SPEED_Msk;
    switch (frequency) {
    case 100:
        i2c_con_reg |= (1 << I2C_I2C_CON_REG_I2C_SPEED_Pos);
        break;
    case 400:
        i2c_con_reg |= (2 << I2C_I2C_CON_REG_I2C_SPEED_Pos);
        break;
    default:
        return HAL_I2C_ERR_INVAL;
    }

    i2c->regs->I2C_CON_REG = i2c_con_reg;

    return 0;
}

int
hal_i2c_init_hw(uint8_t i2c_num, const struct hal_i2c_hw_settings *cfg)
{
    const struct da1469x_hal_i2c *i2c;

    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
       return HAL_I2C_ERR_INVAL;
    }

    i2c_init_hw(i2c, cfg->pin_scl, cfg->pin_sda);

    return 0;
}

int
hal_i2c_config(uint8_t i2c_num, const struct hal_i2c_settings *cfg)
{
    const struct da1469x_hal_i2c *i2c;

    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
       return HAL_I2C_ERR_INVAL;
    }

    return i2c_config(i2c, cfg->frequency);
}

int
hal_i2c_init(uint8_t i2c_num, void *usercfg)
{
    const struct da1469x_hal_i2c_cfg *da1469x_cfg = usercfg;
    const struct da1469x_hal_i2c *i2c;

    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
        return HAL_I2C_ERR_INVAL;
    }

    if (!da1469x_cfg) {
        return HAL_I2C_ERR_INVAL;
    }

    i2c_init_hw(i2c, da1469x_cfg->pin_scl, da1469x_cfg->pin_sda);

    return i2c_config(i2c, da1469x_cfg->frequency);
}

static uint32_t
i2c_get_abort_state(const struct da1469x_hal_i2c *i2c)
{
    return i2c->regs->I2C_TX_ABRT_SOURCE_REG;
}

static bool
i2c_check_tx_is_not_full(const struct da1469x_hal_i2c *i2c)
{
    return i2c->regs->I2C_STATUS_REG & I2C_I2C_STATUS_REG_TFNF_Msk;
}

static bool
i2c_check_tx_is_empty(const struct da1469x_hal_i2c *i2c)
{
    return i2c->regs->I2C_STATUS_REG & I2C_I2C_STATUS_REG_TFE_Msk;
}

static bool
i2c_is_busy(const struct da1469x_hal_i2c *i2c)
{
    return i2c->regs->I2C_STATUS_REG & I2C_I2C_STATUS_REG_MST_ACTIVITY_Msk;
}

static uint8_t
i2c_get_rx_fifo_level(const struct da1469x_hal_i2c *i2c)
{
    return i2c->regs->I2C_RXFLR_REG & I2C_I2C_RXFLR_REG_RXFLR_Msk;
}

static uint8_t
i2c_read_byte(const struct da1469x_hal_i2c *i2c)
{
    return i2c->regs->I2C_DATA_CMD_REG & I2C2_I2C2_DATA_CMD_REG_I2C_DAT_Msk;
}

static void
i2c_set_target_address(const struct da1469x_hal_i2c *i2c, uint16_t address)
{

    if ((i2c->regs->I2C_TAR_REG & I2C_I2C_TAR_REG_IC_TAR_Msk) == address) {
        return;
    }

    /* Disable I2C */
    i2c->regs->I2C_ENABLE_REG &= ~(1 << I2C_I2C_ENABLE_REG_I2C_EN_Pos);

    i2c->regs->I2C_TAR_REG &= ~(I2C_I2C_TAR_REG_IC_TAR_Msk);
    i2c->regs->I2C_TAR_REG |= address;

    /* Enable I2C */
    i2c->regs->I2C_ENABLE_REG |= (1 << I2C_I2C_ENABLE_REG_I2C_EN_Pos);
}

static int
i2c_convert_abort_state_to_err(uint32_t abort_state)
{
    if (abort_state & I2C2_I2C2_TX_ABRT_SOURCE_REG_ABRT_GCALL_NOACK_Msk) {
        return HAL_I2C_ERR_ADDR_NACK;
    }

    if (abort_state & I2C2_I2C2_TX_ABRT_SOURCE_REG_ABRT_TXDATA_NOACK_Msk) {
        return HAL_I2C_ERR_DATA_NACK;
    }

    return HAL_I2C_ERR_UNKNOWN;
}

int
hal_i2c_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                uint32_t timo, uint8_t last_op)
{
    int rc = 0;
    const struct da1469x_hal_i2c *i2c;
    uint32_t start;
    uint32_t abort_state;
    int num_tx;
    uint32_t stop_restart_bits;

    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
       return HAL_I2C_ERR_INVAL;
    }

    /* If in abort state lets reset it */
    if (i2c_get_abort_state(i2c)) {
        (void)i2c->regs->I2C_CLR_TX_ABRT_REG;
    }

    i2c_set_target_address(i2c, pdata->address);

    start = os_time_get();

    num_tx = 0;
    while (num_tx < pdata->len) {
        while (!i2c_check_tx_is_not_full(i2c) && !i2c_get_abort_state(i2c)) {
            if ((os_time_get() - start) > timo) {
                rc = HAL_I2C_ERR_TIMEOUT;
                goto done;
            }
        }

        abort_state = i2c_get_abort_state(i2c);
        if (abort_state) {
            rc = i2c_convert_abort_state_to_err(abort_state);
            goto done;
        }

        stop_restart_bits = 0;
        if ((num_tx == pdata->len - 1) && last_op) {
            stop_restart_bits |= (1 << I2C_I2C_DATA_CMD_REG_I2C_STOP_Pos);
        }

        if (num_tx == 0) {
            stop_restart_bits |= (1 << I2C_I2C_DATA_CMD_REG_I2C_RESTART_Pos);
        }

        i2c->regs->I2C_DATA_CMD_REG = pdata->buffer[num_tx] | stop_restart_bits;

        num_tx++;
    }

    /* If last_op, wait for complete */
    if (last_op) {
        while (!i2c_check_tx_is_empty(i2c)) {
            if (os_time_get() - start > timo) {
                rc = HAL_I2C_ERR_TIMEOUT;
                goto done;
            }
        }

        while (i2c_is_busy(i2c)) {
            if (os_time_get() - start > timo) {
                rc = HAL_I2C_ERR_TIMEOUT;
                goto done;
            }
        }

        abort_state = i2c_get_abort_state(i2c);
        if (abort_state) {
            rc = i2c_convert_abort_state_to_err(abort_state);
            goto done;
        }
    }

done:
    return rc;
}

int
hal_i2c_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                uint32_t timo, uint8_t last_op)
{
    int rc = 0;
    const struct da1469x_hal_i2c *i2c;
    uint32_t start;
    uint32_t abort_state;
    int num_req;
    int num_rx;
    uint32_t stop_restart_bits;

    i2c = hal_i2c_resolve(i2c_num);
    if (!i2c) {
       return HAL_I2C_ERR_INVAL;
    }

    /* If in abort state lets reset it */
    if (i2c_get_abort_state(i2c)) {
        (void)i2c->regs->I2C_CLR_TX_ABRT_REG;
    }

    i2c_set_target_address(i2c, pdata->address);

    start = os_time_get();

    num_req = 0;
    num_rx = 0;

    while (num_rx < pdata->len) {
        while (num_req < pdata->len && i2c_check_tx_is_not_full(i2c)) {
            num_req++;

            stop_restart_bits = 0;
            if ((num_req == pdata->len) && last_op) {
                stop_restart_bits |= (1 << I2C_I2C_DATA_CMD_REG_I2C_STOP_Pos);
            }

            if (num_req == 1) {
                stop_restart_bits |= (1 << I2C_I2C_DATA_CMD_REG_I2C_RESTART_Pos);
            }

            i2c->regs->I2C_DATA_CMD_REG = I2C_I2C_DATA_CMD_REG_I2C_CMD_Msk |
                                            stop_restart_bits;
        }

        while (num_rx < pdata->len && i2c_get_rx_fifo_level(i2c)) {
            pdata->buffer[num_rx] = i2c_read_byte(i2c);
            num_rx++;
        }

        abort_state = i2c_get_abort_state(i2c);
        if (abort_state) {
            rc = i2c_convert_abort_state_to_err(abort_state);
            goto done;
        }

        if ((os_time_get() - start) > timo) {
            rc = HAL_I2C_ERR_TIMEOUT;
            goto done;
        }
    }

done:
    return rc;
}

int
hal_i2c_master_probe(uint8_t i2c_num, uint8_t address, uint32_t timo)
{
    struct hal_i2c_master_data tx;
    uint8_t buf = 0;

    tx.address = address;
    tx.buffer = &buf;
    tx.len = 1;

    /* 
    * Using a i2c write instead of a i2c read because i2c read does not detect any i2c device on bus.
    * Also, i2c read before i2c write causes i2c bus to hang up sometimes. 
    */
    return hal_i2c_master_write(i2c_num, &tx, timo, 1);
}
