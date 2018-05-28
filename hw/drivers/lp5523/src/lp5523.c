/**
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

#include "os/mynewt.h"
#include <hal/hal_i2c.h>
#include <log/log.h>
#include <stats/stats.h>

#include "lp5523/lp5523.h"

#if MYNEWT_VAL(LED_ABSTRACTION_LAYER)
#include "led/led.h"
#endif

/* Define the stats section and records */
STATS_SECT_START(lp5523_stat_section)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(lp5523_stat_section)
    STATS_NAME(lp5523_stat_section, read_errors)
    STATS_NAME(lp5523_stat_section, write_errors)
STATS_NAME_END(lp5523_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(lp5523_stat_section) g_lp5523stats;

#define LOG_MODULE_LP5523    (5523)
#define LP5523_INFO(...)     LOG_INFO(&_log, LOG_MODULE_LP5523, __VA_ARGS__)
#define LP5523_ERR(...)      LOG_ERROR(&_log, LOG_MODULE_LP5523, __VA_ARGS__)
static struct log _log;

int
lp5523_set_reg(struct led_itf *itf, enum lp5523_registers addr,
    uint8_t value)
{
    int rc;
    uint8_t payload[2] = { addr, value };

    struct hal_i2c_master_data data_struct = {
        .address = itf->li_addr,
        .len = 2,
        .buffer = payload
    };

    rc = hal_i2c_master_write(itf->li_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
        LP5523_ERR("Failed to write to 0x%02X:0x%02X with value 0x%02X\n",
                       itf->li_addr, addr, value);
        STATS_INC(g_lp5523stats, read_errors);
    }

    return rc;
}

int
lp5523_get_reg(struct led_itf *itf, enum lp5523_registers addr,
    uint8_t *value)
{
    int rc;

    struct hal_i2c_master_data data_struct = {
        .address = itf->li_addr,
        .len = 1,
        .buffer = &addr
    };

    /* Register write */
    rc = hal_i2c_master_write(itf->li_num, &data_struct,
                              OS_TICKS_PER_SEC / 10, 0);
    if (rc) {
        LP5523_ERR("I2C access failed at address 0x%02X\n", itf->li_addr);
        STATS_INC(g_lp5523stats, write_errors);
        goto err;
    }

    /* Read one byte back */
    data_struct.buffer = value;
    rc = hal_i2c_master_read(itf->li_num, &data_struct,
                             OS_TICKS_PER_SEC / 10, 1);

    if (rc) {
         LP5523_ERR("Failed to read from 0x%02X:0x%02X\n", itf->li_addr, addr);
         STATS_INC(g_lp5523stats, read_errors);
    }

err:
    return rc;
}

static int
lp5523_set_n_regs(struct led_itf *itf, enum lp5523_registers addr,
    uint8_t *vals, uint8_t len)
{
    int rc;
    uint8_t regs[LP5523_MAX_PAYLOAD] = {0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->li_addr,
        .len = len + 1,
        .buffer = regs
    };

    memcpy(regs, vals, len + 1);

    regs[0] = addr;

    rc = hal_i2c_master_write(itf->li_num, &data_struct,
                              (OS_TICKS_PER_SEC / 5), 1);

    if (rc) {
        LP5523_ERR("Failed to write to 0x%02X:0x%02X\n", itf->li_addr, regs[0]);
        STATS_INC(g_lp5523stats, read_errors);
    }

    return rc;
}

/* XXX: Not sure if ai-read is supported, seems to work for reads of length 2 */
static int
lp5523_get_n_regs(struct led_itf *itf, enum lp5523_registers addr,
    uint8_t *vals, uint8_t len)
{
    int rc;
    uint8_t addr_b = (uint8_t) addr;

    struct hal_i2c_master_data data_struct = {
        .address = itf->li_addr,
        .len = 1,
        .buffer = &addr_b
    };

    rc = hal_i2c_master_write(itf->li_num, &data_struct,
        (OS_TICKS_PER_SEC / 10), 0);

    if (rc) {
        LP5523_ERR("Failed to write to 0x%02X:0x%02X\n", itf->li_addr, addr_b);
        STATS_INC(g_lp5523stats, read_errors);
        return rc;
    }

    data_struct.len = len;
    data_struct.buffer = vals;
    rc = hal_i2c_master_read(itf->li_num, &data_struct,
        OS_TICKS_PER_SEC / 5, 1);

    if (rc) {
         LP5523_ERR("Failed to read from 0x%02X:0x%02X\n", itf->li_addr,
            addr_b);
         STATS_INC(g_lp5523stats, read_errors);
    }
    return rc;
}

int
lp5523_calc_temp_comp(float corr_factor, uint8_t *temp_comp)
{
    uint8_t val;

    /* To deactivate compensation set temp_comp to 0 */
    if (corr_factor < -1.5 || corr_factor > 1.5) {
        return SYS_EINVAL;
    }

    val = abs(corr_factor/0.1);

    if (corr_factor < 0) {
        val &= 0x10 & val;
    }

    return 0;
}


static void
lp5523_wait(uint32_t ms)
{
    os_time_delay(((ms * OS_TICKS_PER_SEC) / 1000) + 1);
}

int lp5523_apply_value(struct lp5523_register_value addr, uint8_t value,
    uint8_t *reg)
{
    value <<= addr.pos;

    if ((value & (~addr.mask)) != 0) {
        return -1;
    }

    *reg &= ~addr.mask;
    *reg |= value;

    return 0;
}

int
lp5523_set_value(struct led_itf *itf, struct lp5523_register_value addr,
    uint8_t value)
{
    int rc;
    uint8_t reg;

    rc = lp5523_get_reg(itf, addr.reg, &reg);
    if (rc) {
        return rc;
    }

    rc = lp5523_apply_value(addr, value, &reg);
    if (rc) {
        return rc;
    }

    return lp5523_set_reg(itf, addr.reg, reg);
}

int
lp5523_get_value(struct led_itf *itf, struct lp5523_register_value addr,
    uint8_t *value)
{
    int rc;
    uint8_t reg;

    rc = lp5523_get_reg(itf, addr.reg, &reg);

    *value = (reg & addr.mask) >> addr.pos;

    return rc;
}

int
lp5523_set_bitfield(struct led_itf *itf, enum lp5523_bitfield_registers addr,
    uint16_t outputs)
{
    uint8_t vals[2] = { (outputs >> 8) & 0x01, outputs & 0xff };

    return lp5523_set_n_regs(itf, addr, vals, 2);
}

int
lp5523_get_bitfield(struct led_itf *itf, enum lp5523_bitfield_registers addr,
    uint16_t* outputs)
{
    int rc;
    uint8_t vals[2];

    rc = lp5523_get_n_regs(itf, addr, vals, 2);

    *outputs = ((vals[0] & 0x01) << 8) | vals[1];

    return rc;
}

int
lp5523_set_output_on(struct led_itf *itf, uint8_t output, uint8_t on)
{
    int rc;
    uint16_t outputs;

    if ((output < 1) || (output > 9)) {
        rc = -1;
        goto err;
    }

    rc = lp5523_get_bitfield(itf, LP5523_OUTPUT_CTRL_MSB,
                             &outputs);
    if (rc) {
        goto err;
    }

    if (!on) {
        outputs &= ~(0x1 << (output - 1));
    } else {
        outputs |= (0x1 << (output - 1));
    }

    rc = lp5523_set_bitfield(itf, LP5523_OUTPUT_CTRL_MSB, outputs);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
lp5523_get_output_on(struct led_itf *itf, uint8_t output, uint8_t *on)
{
    int rc;
    uint16_t outputs;

    if ((output < 1) || (output > 9)) {
        rc = -1;
        goto err;
    }

    rc = lp5523_get_bitfield(itf, LP5523_OUTPUT_CTRL_MSB,
                             &outputs);
    if (rc) {
        goto err;
    }

    *on = (outputs & (0x1 << (output - 1)));

    return 0;
err:
    return rc;
}

int
lp5523_set_output_reg(struct led_itf *itf, enum lp5523_output_registers addr,
    uint8_t output, uint8_t value)
{
    if ((output < 1) || (output > 9)) {
        return -1;
    }

    return lp5523_set_reg(itf, addr + (output - 1), value);
}

int
lp5523_get_output_reg(struct led_itf *itf, enum lp5523_output_registers addr,
    uint8_t output, uint8_t *value)
{
    if ((output < 1) || (output > 9)) {
        return -1;
    }
    return lp5523_get_reg(itf, addr + (output - 1), value);
}


int
lp5523_set_engine_reg(struct led_itf *itf, enum lp5523_engine_registers addr,
    uint8_t engine, uint8_t value)
{
    if ((engine < 1) || (engine > 3)) {
        return -1;
    }
    return lp5523_set_reg(itf, addr + (engine - 1), value);
}

int
lp5523_get_engine_reg(struct led_itf *itf, enum lp5523_engine_registers addr,
    uint8_t engine, uint8_t *value)
{
    if ((engine < 1) || (engine > 3)) {
        return -1;
    }
    return lp5523_get_reg(itf, addr + (engine - 1), value);
}

int
lp5523_set_enable(struct led_itf *itf, uint8_t enable)
{
    int rc;

    rc = lp5523_set_value(itf, LP5523_CHIP_EN, enable);

    if ((rc == 0) && (enable)) {
        lp5523_wait(1);
    }
    return rc;
}

int
lp5523_set_engine_control(struct led_itf *itf,
    enum lp5523_engine_control_registers addr, uint8_t engine_mask, uint8_t values)
{
    uint8_t reg;
    int rc;

    if (((engine_mask & LP5523_ENGINE1_MASK) != LP5523_ENGINE1_MASK) &&
        ((engine_mask & LP5523_ENGINE2_MASK) != LP5523_ENGINE2_MASK) &&
        ((engine_mask & LP5523_ENGINE3_MASK) != LP5523_ENGINE3_MASK)) {
        return -1;
    }

    rc = lp5523_get_reg(itf, addr, &reg);
    if (rc) {
        return rc;
    }

    reg &= ~engine_mask;
    reg |= engine_mask & values;

    return lp5523_set_reg(itf, addr, reg);
}

int
lp5523_set_output_mapping(struct led_itf *itf, uint8_t output,
    uint8_t mapping)
{
    struct lp5523_register_value reg;

    if ((output < 1) || (output > 9)) {
        return -1;
    }

    reg = LP5523_OUTPUT_MAPPING;
    reg.reg += output - 1;

    return lp5523_set_value(itf, reg, mapping);
}

int
lp5523_get_output_mapping(struct led_itf *itf, uint8_t output,
    uint8_t *mapping)
{
    struct lp5523_register_value reg;

    if ((output < 1) || (output > 9)) {
        return -1;
    }

    reg = LP5523_OUTPUT_MAPPING;
    reg.reg += output - 1;

    return lp5523_get_value(itf, reg, mapping);
}

int
lp5523_set_output_log_dim(struct led_itf *itf, uint8_t output,
    uint8_t enable)
{
    struct lp5523_register_value reg;

    if ((output < 1) || (output > 9)) {
        return -1;
    }

    reg = LP5523_OUTPUT_LOG_EN;
    reg.reg += output - 1;

    return lp5523_set_value(itf, reg, enable);
}

int
lp5523_get_output_log_dim(struct led_itf *itf, uint8_t output,
    uint8_t *enable)
{
    struct lp5523_register_value reg;

    if ((output < 1) || (output > 9)) {
        return -1;
    }

    reg = LP5523_OUTPUT_LOG_EN;
    reg.reg += output - 1;

    return lp5523_get_value(itf, reg, enable);
}

int
lp5523_set_output_temp_comp(struct led_itf *itf, uint8_t output,
    uint8_t value)
{
    struct lp5523_register_value reg;

    if ((output < 1) || (output > 9)) {
        return -1;
    }

    reg = LP5523_OUTPUT_TEMP_COMP;
    reg.reg += output - 1;

    return lp5523_set_value(itf, reg, value);
}

int
lp5523_get_output_temp_comp(struct led_itf *itf, uint8_t output,
    uint8_t *value)
{
    struct lp5523_register_value reg;

    if ((output < 1) || (output > 9)) {
        return -1;
    }

    reg = LP5523_OUTPUT_TEMP_COMP;
    reg.reg += output - 1;

    return lp5523_get_value(itf, reg, value);
}

int
lp5523_get_engine_int(struct led_itf *itf, uint8_t engine, uint8_t *flag)
{
    struct lp5523_register_value reg;

    if ((engine < 1) || (engine > 3)) {
        return -1;
    }

    reg = LP5523_ENG1_INT;
    reg.pos >>= engine - 1;

    return lp5523_get_value(itf, reg, flag);
}

int
lp5523_reset(struct led_itf *itf)
{
    return lp5523_set_reg(itf, LP5523_RESET, 0xff);
}

int
lp5523_set_page_sel(struct led_itf *itf, uint8_t page)
{
    if (page > 5) {
        return -1;
    }
    return lp5523_set_reg(itf, LP5523_PROG_MEM_PAGE_SEL, page);
}

int
lp5523_set_engine_mapping(struct led_itf *itf, uint8_t engine,
    uint16_t outputs)
{
    if ((engine < 1) || (engine > 3)) {
        return -1;
    }

    return lp5523_set_bitfield(itf, LP5523_ENG_MAPPING + ((engine - 1) << 1),
        outputs);
}

int
lp5523_get_engine_mapping(struct led_itf *itf, uint8_t engine,
    uint16_t *outputs)
{
    if ((engine < 1) || (engine > 3)) {
        return -1;
    }

    return lp5523_get_bitfield(itf, LP5523_ENG_MAPPING + ((engine - 1) << 1),
        outputs);
}

static int
lp5523_set_pr_instruction(struct led_itf *itf, uint8_t addr, uint16_t *ins)
{
    uint8_t mem[2] = { ((*ins) >> 8) & 0x00ff, (*ins) & 0x00ff };

    return lp5523_set_n_regs(itf, LP5523_PROGRAM_MEMORY + (addr << 1), mem, 2);
}

static int
lp5523_get_pr_instruction(struct led_itf *itf, uint8_t addr, uint16_t *ins)
{
    int rc;
    uint8_t mem[2];

    rc = lp5523_get_n_regs(itf, LP5523_PROGRAM_MEMORY + (addr << 1), mem, 2);

    *ins = (((uint16_t)mem[0]) << 8) | mem[1];

    return rc;
}

static int
lp5523_verify_pr_instruction(struct led_itf *itf, uint8_t addr,
    uint16_t *ins)
{
    int rc;
    uint8_t mem[2];
    uint16_t ver;

    rc = lp5523_get_n_regs(itf, LP5523_PROGRAM_MEMORY + (addr << 1), mem, 2);
    if (rc) {
        return rc;
    }

    ver = ((((uint16_t)mem[0]) << 8) | mem[1]);

    return (*ins != ver) ? 1 : 0;
}


static int
lp5523_rwv_page(struct led_itf *itf, int(*irwv)(struct led_itf *,
    uint8_t, uint16_t *), uint8_t page, uint16_t* pgm,
    uint8_t start, uint8_t size)
{
    int rc;
    uint8_t i;

    rc = lp5523_set_page_sel(itf, page);
    if (rc) {
        return rc;
    }

    i = 0;
    while (i < size) {
        rc = irwv(itf, start, pgm + i);
        if (rc) {
            return rc;
        }
        ++start;
        ++i;
    }
    return 0;
}

static int
lp5523_set_page(struct led_itf *itf, uint8_t page, uint16_t* pgm,
    uint8_t start, uint8_t size)
{
    return lp5523_rwv_page(itf, &lp5523_set_pr_instruction, page, pgm, start,
        size);
}

static int
lp5523_get_page(struct led_itf *itf, uint8_t page, uint16_t* pgm,
    uint8_t start, uint8_t size)
{
    return lp5523_rwv_page(itf, &lp5523_get_pr_instruction, page, pgm, start,
        size);
}

static int
lp5523_verify_page(struct led_itf *itf, uint8_t page, uint16_t* pgm,
    uint8_t start, uint8_t size)
{
    return lp5523_rwv_page(itf, &lp5523_verify_pr_instruction, page, pgm, start,
        size);
}

static int
lp5523_rwv_program(struct led_itf *itf, int(*prw)(struct led_itf *,
    uint8_t, uint16_t *, uint8_t, uint8_t), uint16_t* pgm, uint8_t start,
    uint8_t size)
{
    int rc;
    uint8_t page;
    uint8_t last_page;
    uint8_t start_rel;
    uint8_t end;

    if (((start + size) > LP5523_MEMORY_SIZE) || (size == 0)) {
        return -1;
    }

    end = start + size;
    page = start / LP5523_PAGE_SIZE;
    last_page = (end - 1) / LP5523_PAGE_SIZE;
    start_rel = start % LP5523_PAGE_SIZE;

    if (page == last_page) {
        rc = prw(itf, page, pgm, start_rel, size);
        if (rc) {
            return rc;
        }
    } else {
        uint8_t end_rel;
        uint8_t write_size;

        write_size = LP5523_PAGE_SIZE - start_rel;
        rc = prw(itf, page, pgm, start_rel, write_size);
        if (rc) {
            return rc;
        }
        ++page;
        pgm += write_size;

        while (page < last_page) {
            rc = prw(itf, page, pgm, 0, LP5523_PAGE_SIZE);
            if (rc) {
                return rc;
            }
            ++page;
            pgm += LP5523_PAGE_SIZE;
        }

        end_rel = end % LP5523_PAGE_SIZE;
        write_size = (end_rel != 0) ? end_rel : LP5523_PAGE_SIZE;
        return prw(itf, page, pgm, 0, write_size);
    }
    return 0;
}

int
lp5523_set_instruction(struct led_itf *itf, uint8_t addr, uint16_t ins)
{
    int rc;

    rc = lp5523_set_page_sel(itf, addr / LP5523_PAGE_SIZE);
    return (rc) ? rc : lp5523_set_pr_instruction(itf,
        addr % LP5523_PAGE_SIZE, &ins);
}

int
lp5523_get_instruction(struct led_itf *itf, uint8_t addr, uint16_t *ins)
{
    int rc;

    rc = lp5523_set_page_sel(itf, addr / LP5523_PAGE_SIZE);
    return (rc) ? rc : lp5523_get_pr_instruction(itf,
        addr % LP5523_PAGE_SIZE, ins);
}

int
lp5523_set_program(struct led_itf *itf, uint8_t engine_mask, uint16_t *pgm,
    uint8_t start, uint8_t size)
{
    int rc;

    if (((start + size) > LP5523_MEMORY_SIZE) || (size == 0)) {
        return -1;
    }

    /* disable engines */
    rc = lp5523_set_engine_control(itf, LP5523_ENGINE_CNTRL2, engine_mask,
        LP5523_ENGINES_DISABLED);
    if (rc) {
        return rc;
    }

    /* put engines into load program state */
    rc = lp5523_set_engine_control(itf, LP5523_ENGINE_CNTRL2, engine_mask,
        LP5523_ENGINES_LOAD_PROGRAM);
    if (rc) {
        return rc;
    }

    /* wait for engine busy */
    lp5523_wait(1);

    return lp5523_rwv_program(itf, &lp5523_set_page, pgm, start, size);
}

int
lp5523_get_program(struct led_itf *itf, uint16_t *pgm, uint8_t start,
    uint8_t size)
{
    return lp5523_rwv_program(itf, &lp5523_get_page, pgm, start, size);
}

int
lp5523_verify_program(struct led_itf *itf, uint16_t *pgm, uint8_t start,
    uint8_t size)
{
    return lp5523_rwv_program(itf, &lp5523_verify_page, pgm, start, size);
}

int
lp5523_engines_run(struct led_itf *itf, uint8_t engine_mask)
{
    int rc;

    rc = lp5523_set_engine_control(itf, LP5523_ENGINE_CNTRL1, engine_mask,
        LP5523_ENGINES_FREE_RUN);
    return (rc) ? rc : lp5523_set_engine_control(itf, LP5523_ENGINE_CNTRL2,
        engine_mask, LP5523_ENGINES_RUN_PROGRAM);
}

int
lp5523_engines_hold(struct led_itf *itf, uint8_t engine_mask)
{
    return lp5523_set_engine_control(itf, LP5523_ENGINE_CNTRL1, engine_mask,
        LP5523_ENGINES_HOLD);
}

int
lp5523_engines_step(struct led_itf *itf, uint8_t engine_mask)
{
    return lp5523_set_engine_control(itf, LP5523_ENGINE_CNTRL1, engine_mask,
        LP5523_ENGINES_STEP);
}

int
lp5523_engines_disable(struct led_itf *itf, uint8_t engine_mask)
{
    return lp5523_set_engine_control(itf, LP5523_ENGINE_CNTRL2, engine_mask,
        LP5523_ENGINES_DISABLED);
}

int
lp5523_read_adc(struct led_itf *itf, uint8_t pin, uint8_t *v) {
    int rc;

    if (pin > 0x1f) {
        return -1;
    }

    rc = lp5523_set_reg(itf, LP5523_LED_TEST_CONTROL,
        pin | LP5523_EN_LED_TEST_ADC.mask);
    if (rc) {
        return rc;
    }

    lp5523_wait(3);

    return lp5523_get_reg(itf, LP5523_LED_TEST_ADC, v);
}

int
lp5523_get_status(struct led_itf *itf, uint8_t *status)
{
    uint8_t reg;
    int rc;

    rc = lp5523_get_reg(itf, LP5523_STATUS, &reg);
    if (rc) {
        return rc;
    }

    *status = reg;

    return 0;
}

int
lp5523_self_test(struct led_itf *itf) {
    int rc;
    uint8_t status;
    uint8_t misc;
    uint8_t vdd;
    uint8_t adc;
    uint8_t i;

    rc = lp5523_get_status(itf, &status);
    if (rc) {
        return rc;
    }

    rc = lp5523_get_reg(itf, LP5523_MISC, &misc);
    if (rc) {
        return rc;
    }

    /* if external clock is forced, check it is detected */
    if ((((misc & (LP5523_CLK_DET_EN.mask | LP5523_INT_CLK_EN.mask))) == 0) &&
        ((status & LP5523_EXT_CLK_USED.mask) == 0)) {
        return -2;
    }

    /* measure VDD */
    rc = lp5523_read_adc(itf, LP5523_LED_TEST_VDD, &vdd);
    if (rc) {
        return rc;
    }

    i = 1;
    while (i <= 9) {
        /* set LED PWM to 0xff */
        rc = lp5523_set_output_reg(itf, LP5523_PWM, i, 0xff);
        if (rc) {
            return rc;
        }

        /* wait to stabilise */
        lp5523_wait(4);

        /* read the ADC */
        rc = lp5523_read_adc(itf, i - 1, &adc);
        if (rc) {
            return rc;
        }
        if ((adc > vdd) || (adc < LP5523_LED_TEST_SC_LIM)) {
            return -3;
        }

        rc = lp5523_set_output_reg(itf, LP5523_PWM, i, 0x00);
        if (rc) {
            return rc;
        }
        ++i;
    }

    return 0;
}

int
lp5523_init(struct os_dev *dev, void *arg)
{
    int rc;

    if (!arg || !dev) {
        return SYS_ENODEV;
    }

    log_register(dev->od_name, &_log, &log_console_handler, NULL,
        LOG_SYSLEVEL);

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_lp5523stats),
        STATS_SIZE_INIT_PARMS(g_lp5523stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lp5523_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_lp5523stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    return 0;
}

int
lp5523_get_output_curr_ctrl(struct led_itf *itf, uint8_t output, uint8_t *curr_ctrl)
{
      int rc;
      uint8_t reg;

      rc = lp5523_get_output_reg(itf, LP5523_CURRENT_CONTROL, output, &reg);
      if (rc) {
          goto err;
      }

      *curr_ctrl = reg;
err:
      return rc;
}

int
lp5523_set_output_curr_ctrl(struct led_itf *itf, uint8_t output, uint8_t curr_ctrl)
{

      return lp5523_set_output_reg(itf, LP5523_CURRENT_CONTROL, output, curr_ctrl);
}

int
lp5523_config(struct lp5523 *lp5523, struct lp5523_cfg *cfg)
{
    int rc;
    int i;
    struct led_itf *itf;
    uint8_t misc_val;

    itf = LED_GET_ITF(&(lp5523->led_dev));

    itf->li_addr = LP5523_I2C_BASE_ADDR + cfg->asel;

    rc = lp5523_reset(itf);
    if (rc) {
        return rc;
    }

    /* chip enable */
    rc = lp5523_set_enable(itf, 1);
    if (rc) {
        goto err;
    }

    misc_val = 0;
    if (cfg->auto_inc_en) {
        misc_val = LP5523_EN_AUTO_INCR.mask;
    } else {
        misc_val = LP5523_CLK_DET_EN.mask;
    }

    rc = lp5523_apply_value(LP5523_CLK_DET_EN, cfg->clk_det_en, &misc_val);
    if (rc) {
        goto err;
    }

    rc = lp5523_apply_value(LP5523_INT_CLK_EN, cfg->int_clk_en, &misc_val);
    if (rc) {
        goto err;
    }

    rc = lp5523_apply_value(LP5523_VARIABLE_D_SEL, cfg->var_d_sel, &misc_val);
    if (rc) {
        goto err;
    }

    rc = lp5523_apply_value(LP5523_POWERSAVE_EN, cfg->ps_en, &misc_val);
    if (rc) {
        goto err;
    }

    rc = lp5523_apply_value(LP5523_PWM_PS_EN, cfg->pwm_ps_en, &misc_val);
    if (rc) {
        goto err;
    }

    rc = lp5523_apply_value(LP5523_CP_MODE, cfg->cp_mode, &misc_val);
    if (rc) {
        goto err;
    }

    rc = lp5523_set_reg(itf, LP5523_MISC, misc_val);
    if (rc) {
        goto err;
    }

    rc = lp5523_set_value(itf, LP5523_INT_CONF, cfg->int_conf);
    if (rc) {
        goto err;
    }

    rc = lp5523_set_value(itf, LP5523_INT_GPO, cfg->int_gpo);
    if (rc) {
        goto err;
    }

    for (i = 1; i <= MYNEWT_VAL(LEDS_PER_DRIVER); i++) {
        rc = lp5523_set_output_curr_ctrl(itf, i, cfg->per_led_cfg[i - 1].current_ctrl);
        if (rc) {
            goto err;
        }

        lp5523_wait(1);

        rc = lp5523_set_output_log_dim(itf, i, cfg->per_led_cfg[i - 1].log_dim_en);
        if (rc) {
            goto err;
        }

        lp5523_wait(1);

        rc = lp5523_set_output_temp_comp(itf, i, cfg->per_led_cfg[i - 1].temp_comp);
        if (rc) {
            goto err;
        }

        lp5523_wait(1);

        rc = lp5523_set_output_on(itf, i, cfg->per_led_cfg[i - 1].output_on);
        if (rc) {
            goto err;
        }

        lp5523_wait(1);
    }

err:
    return rc;
}
