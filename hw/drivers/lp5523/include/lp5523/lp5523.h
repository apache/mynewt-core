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

#ifndef __LP5523_H__
#define __LP5523_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sensor/sensor.h>

#if MYNEWT_VAL(LED_ENABLE_ABSTRACTION)
#include <led/led.h>
#endif

#define LP5523_MAX_PAYLOAD   (10)

#define LP5523_I2C_BASE_ADDR (0x32)

/* Engine control mask */
#define LP5523_ENGINE3_MASK (0x03)
#define LP5523_ENGINE2_MASK (0x0c)
#define LP5523_ENGINE1_MASK (0x30)

/* Engine IDs */
#define LP5523_ENGINE3 (3)
#define LP5523_ENGINE2 (2)
#define LP5523_ENGINE1 (1)

/* LED IDs */
#define LP5523_LED9 (9)
#define LP5523_LED8 (8)
#define LP5523_LED7 (7)
#define LP5523_LED6 (6)
#define LP5523_LED5 (5)
#define LP5523_LED4 (4)
#define LP5523_LED3 (3)
#define LP5523_LED2 (2)
#define LP5523_LED1 (1)

struct per_led_cfg {
    /* Mapping */
    uint8_t mapping:2;
    /* Enable Log dimming */
    uint8_t log_dim_en:1;
    /* Enable temperature compensation */
    uint8_t temp_comp:5;
    /* Output On/Off */
    uint8_t output_on:1;
    /* In steps of 100 microamps */
    uint8_t current_ctrl;
};

struct lp5523_cfg {
    /* The 2 LSBs of this represent ASEL1 and ASEL0 */
    uint8_t asel:2;
    /* Enable clock detect enable */
    uint8_t clk_det_en:1;
    /* Enable Int clock */
    uint8_t int_clk_en:1;
    /* Select CP Mode */
    uint8_t cp_mode:2;
    /* Enable VAR_D_SEL */
    uint8_t var_d_sel:1;
    /* Enable Power Save */
    uint8_t ps_en:1;
    /* Enable PWM power save */
    uint8_t pwm_ps_en:1;
    /* Enable auto increment */
    uint8_t auto_inc_en:1;
    /* INT conf */
    uint8_t int_conf:1;
    /* INT GPO */

    /****** Gain change control settings ******/
    uint8_t int_gpo:1;
    /* Threshold Voltage */
    uint8_t thresh_volt:2;
    /* Enable adaptive threshold */
    uint8_t adapt_thresh_en:1;
    /* Timer value */
    uint8_t timer:2;
    /* Force_1x enbale */
    uint8_t force_1x:1;

    /* All per LED configs go here - 0: D1 8: D9 */
    struct per_led_cfg per_led_cfg[MYNEWT_VAL(LP5523_LEDS_PER_DRIVER)];
};

struct lp5523 {
    struct os_dev dev;
#if MYNEWT_VAL(LED_ENABLE_ABSTRACTION)
    struct led_dev led_dev;
#endif
    struct lp5523_cfg cfg;
};

#if MYNEWT_VAL(LED_ENABLE_ABSTRACTION) == 0
/*
 * LED interface
 */
struct led_itf {

    /* LED interface type */
    uint8_t li_type;

    /* interface number */
    uint8_t li_num;

    /* CS pin - optional, only needed for SPI */
    uint8_t li_cs_pin;

    /* LED chip address, only needed for I2C */
    uint16_t li_addr;
};
#endif

/**** Config Values ****/
#define LP5523_ASEL00_ADDR_32h            0x00
#define LP5523_ASEL01_ADDR_33h            0x01
#define LP5523_ASEL10_ADDR_34h            0x02
#define LP5523_ASEL11_ADDR_35h            0x03

#define LP5523_CP_MODE_OFF                0x00
#define LP5523_CP_MODE_FORCE_TO_BYPASS    0x01
#define LP5523_CP_MODE_FORCE_TO_1_5X      0x02
#define LP5523_CP_MODE_AUTO               0x03

#define LP5523_THRESH_VOLT_400MV          0x00
#define LP5523_THRESH_VOLT_300MV          0x01
#define LP5523_THRESH_VOLT_200MV          0x02
#define LP5523_THRESH_VOLT_100MV          0x03

#define LP5523_TIMER_5MS                  0x00
#define LP5523_TIMER_10MS                 0x01
#define LP5523_TIMER_50MS                 0x02
#define LP5523_TIMER_INF                  0x03

/* register addresses */

enum lp5523_bitfield_registers {
    LP5523_OUTPUT_RATIOMETRIC = 0x02,
    LP5523_OUTPUT_CTRL_MSB = 0x04,
    LP5523_ENG_MAPPING = 0x70
};

enum lp5523_output_registers {
    LP5523_CONTROL = 0x06,
    LP5523_PWM = 0x16,
    LP5523_CURRENT_CONTROL = 0x26
};

enum lp5523_engine_registers {
    LP5523_ENGINE_PC = 0x37,
    LP5523_ENGINE_VARIABLE_A = 0x45,
    LP5523_MASTER_FADER = 0x48,
    LP5523_ENG_PROG_START_ADDR = 0x4c
};

enum lp5523_engine_control_registers {
    LP5523_ENGINE_CNTRL1 = 0x00,
    LP5523_ENGINE_CNTRL2 = 0x01
};

enum lp5523_registers {
    LP5523_ENABLE = 0x00,
    LP5523_OUTPUT_CTRL_LSB = 0x05,
    LP5523_LED_CONTROL_BASE = 0x06,
    LP5523_PWM_BASE = 0x16,
    LP5523_MISC = 0x36,
    LP5523_STATUS = 0x3a,
    LP5523_INTERRUPT = 0x3a,
    LP5523_INT = 0x3b,
    LP5523_VARIABLE = 0x3c,
    LP5523_RESET = 0x3d,
    LP5523_TEMP_ADC_CONTROL = 0x3e,
    LP5523_TEMPERATURE_READ = 0x3f,
    LP5523_TEMPERATURE_WRITE = 0x40,
    LP5523_LED_TEST_CONTROL = 0x41,
    LP5523_LED_TEST_ADC = 0x42,
    LP5523_ENG1_PROG_START_ADDR = 0x4c,
    LP5523_ENG2_PROG_START_ADDR = 0x4d,
    LP5523_ENG3_PROG_START_ADDR = 0x4e,
    LP5523_PROG_MEM_PAGE_SEL = 0x4f,
    LP5523_LED_MASTER_FADER1 = 0x48,
    LP5523_LED_MASTER_FADER2 = 0x49,
    LP5523_LED_MASTER_FADER3 = 0x4A,
    LP5523_PROGRAM_MEMORY = 0x50,
    LP5523_ENG1_MAPPING_MSB = 0X70,
    LP5523_ENG2_MAPPING_MSB = 0X72,
    LP5523_ENG3_MAPPING_MSB = 0X74,
    LP5523_GAIN_CHANGE_CTRL = 0x76
};

/* registers */
struct lp5523_register_value {
    uint8_t reg;
    uint8_t pos;
    uint8_t mask;
};

#define LP5523_REGISTER_VALUE(r, n, p, m) \
static const struct lp5523_register_value n = { .reg = r, .pos = p, .mask = m }

/* control1 */
LP5523_REGISTER_VALUE(LP5523_ENABLE, LP5523_CHIP_EN, 6, 0x40);

#define LP5523_ENGINES_HOLD (0x00)
#define LP5523_ENGINES_STEP (0x15)
#define LP5523_ENGINES_FREE_RUN (0x2a)
#define LP5523_ENGINES_EXECUTE_ONCE (0x3f)

/* control2 */
#define LP5523_ENGINES_DISABLED (0x00)
#define LP5523_ENGINES_LOAD_PROGRAM (0x15)
#define LP5523_ENGINES_RUN_PROGRAM (0x2a)
#define LP5523_ENGINES_HALT (0x3f)

/* output control */
LP5523_REGISTER_VALUE(LP5523_CONTROL, LP5523_OUTPUT_MAPPING, 6, 0xc0);
LP5523_REGISTER_VALUE(LP5523_CONTROL, LP5523_OUTPUT_LOG_EN, 5, 0x20);
LP5523_REGISTER_VALUE(LP5523_CONTROL, LP5523_OUTPUT_TEMP_COMP, 0, 0x1f);

/* misc */
LP5523_REGISTER_VALUE(LP5523_MISC, LP5523_VARIABLE_D_SEL, 7, 0x80);
LP5523_REGISTER_VALUE(LP5523_MISC, LP5523_EN_AUTO_INCR, 6, 0x40);
LP5523_REGISTER_VALUE(LP5523_MISC, LP5523_POWERSAVE_EN, 5, 0x20);
LP5523_REGISTER_VALUE(LP5523_MISC, LP5523_CP_MODE, 3, 0x18);

#define LP5523_CP_OFF (0x00)
#define LP5523_CP_FORCED_BYPASS (0x01)
#define LP5523_CP_FORCED_BOOST (0x02)
#define LP5523_CP_AUTOMATIC (0x03)

LP5523_REGISTER_VALUE(LP5523_MISC, LP5523_PWM_PS_EN, 2, 0x04);
LP5523_REGISTER_VALUE(LP5523_MISC, LP5523_CLK_DET_EN, 1, 0x02);
LP5523_REGISTER_VALUE(LP5523_MISC, LP5523_INT_CLK_EN, 0, 0x01);

/* status */
LP5523_REGISTER_VALUE(LP5523_STATUS, LP5523_MEAS_DONE, 7, 0x80);
LP5523_REGISTER_VALUE(LP5523_STATUS, LP5523_MASK_BUSY, 6, 0x40);
LP5523_REGISTER_VALUE(LP5523_STATUS, LP5523_STARTUP_BUSY, 5, 0x20);
LP5523_REGISTER_VALUE(LP5523_STATUS, LP5523_ENGINE_BUSY, 4, 0x10);
LP5523_REGISTER_VALUE(LP5523_STATUS, LP5523_EXT_CLK_USED, 3, 0x08);
LP5523_REGISTER_VALUE(LP5523_STATUS, LP5523_ENG1_INT, 2, 0x04);
LP5523_REGISTER_VALUE(LP5523_STATUS, LP5523_ENG2_INT, 1, 0x02);
LP5523_REGISTER_VALUE(LP5523_STATUS, LP5523_ENG3_INT, 0, 0x01);

/* INT */
LP5523_REGISTER_VALUE(LP5523_INT, LP5523_INT_CONF, 2, 0x04);
LP5523_REGISTER_VALUE(LP5523_INT, LP5523_INT_GPO, 0, 0x01);

/* temp ADC control */
LP5523_REGISTER_VALUE(LP5523_TEMP_ADC_CONTROL, LP5523_TEMP_MEAS_BUSY, 7, 0x80);
LP5523_REGISTER_VALUE(LP5523_TEMP_ADC_CONTROL, LP5523_EN_TEMP_SENSOR, 2, 0x04);
LP5523_REGISTER_VALUE(LP5523_TEMP_ADC_CONTROL, LP5523_TEMP_CONTINUOUS_CONV, 1,
    0x02);
LP5523_REGISTER_VALUE(LP5523_TEMP_ADC_CONTROL, LP5523_SEL_EXT_TEMP, 0, 0x01);

/* LED test control */
LP5523_REGISTER_VALUE(LP5523_LED_TEST_CONTROL, LP5523_EN_LED_TEST_ADC, 7, 0x80);
LP5523_REGISTER_VALUE(LP5523_LED_TEST_CONTROL, LP5523_EN_LED_TEST_INT, 6, 0x40);
LP5523_REGISTER_VALUE(LP5523_LED_TEST_CONTROL, LP5523_LED_CONTINUOUS_CONV, 5,
    0x20);
LP5523_REGISTER_VALUE(LP5523_LED_TEST_CONTROL, LP5523_LED_LED_TEST, 0, 0x1f);

#define LP5523_LED_TEST_D1 (0x00)
#define LP5523_LED_TEST_D2 (0x01)
#define LP5523_LED_TEST_D3 (0x02)
#define LP5523_LED_TEST_D4 (0x03)
#define LP5523_LED_TEST_D5 (0x04)
#define LP5523_LED_TEST_D6 (0x05)
#define LP5523_LED_TEST_D7 (0x06)
#define LP5523_LED_TEST_D8 (0x07)
#define LP5523_LED_TEST_D9 (0x08)
#define LP5523_LED_TEST_VOUT (0x0f)
#define LP5523_LED_TEST_VDD (0x10)
#define LP5523_LED_TEST_INT (0x11)

#define LP5523_LED_TEST_SC_LIM (80)

/* program memory */
#define LP5523_PAGE_SIZE (0x10)
#define LP5523_MEMORY_SIZE (LP5523_PAGE_SIZE * 6)

/* gain change ctrl */
LP5523_REGISTER_VALUE(LP5523_GAIN_CHANGE_CTRL, LP5523_THRESHOLD_MASK, 6, 0xc0);
LP5523_REGISTER_VALUE(LP5523_GAIN_CHANGE_CTRL, LP5523_ADAPTIVE_THRESH_EN, 5,
    0x20);
LP5523_REGISTER_VALUE(LP5523_GAIN_CHANGE_CTRL, LP5523_TIMER, 3, 0x18);
LP5523_REGISTER_VALUE(LP5523_GAIN_CHANGE_CTRL, LP5523_FORCE_1X, 2, 0x04);

/**
 * Writes a single byte to the specified register.
 *
 * @param The sensor interface.
 * @param The register address to write to.
 * @param The value to write.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_reg(struct led_itf *itf, enum lp5523_registers addr,
    uint8_t value);

/**
 * Reads a single byte from the specified register.
 *
 * @param The sensor interface.
 * @param The register address to read from.
 * @param Pointer to where the register value should be written.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_get_reg(struct led_itf *itf, enum lp5523_registers addr,
    uint8_t *value);

/**
 * Applies a value to a position in a local register value.
 *
 * @param A struct containing the position and mask to use for the write.
 * @param The value to write.
 * @param Pointer to register to be modified.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_apply_value(struct lp5523_register_value addr, uint8_t value,
        uint8_t *reg);

/**
 * Writes a section of the specified register.
 *
 * @param The sensor interface.
 * @param The register address to write to.
 * @param The mask.
 * @param Value to write.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_value(struct led_itf *itf, struct lp5523_register_value addr,
    uint8_t value);

/**
 * Reads a section from the specified register.
 *
 * @param The sensor interface.
 * @param The register address to read from.
 * @param The mask.
 * @param Pointer to where the value should be written.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_get_value(struct led_itf *itf, struct lp5523_register_value addr,
    uint8_t *value);

/**
 * Writes 9 bits to 2 registers.
 * To write to:
 * LP5523_BF_OUTPUT_RATIOMETRIC
 * LP5523_BF_OUTPUT_CONTROL
 * LP5523_BF_ENG_MAPPING (see lp5523_set_engine_mapping())
 *
 * @param The sensor interface.
 * @param The register address to write to (the MSB register).
 * @param The values to write, only the least significant 9 bits are
 * significant.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_bitfield(struct led_itf *itf,
    enum lp5523_bitfield_registers addr, uint16_t outputs);

/**
 * Writes 9 bits to 2 registers.
 * To read from:
 * LP5523_BF_OUTPUT_RATIOMETRIC
 * LP5523_BF_OUTPUT_CONTROL
 * LP5523_BF_ENG_MAPPING (see lp5523_set_engine_mapping())
 *
 * @param The sensor interface.
 * @param The register address to read from (the MSB register).
 * @param Pointer to where the register values should be written.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_get_bitfield(struct led_itf *itf,
    enum lp5523_bitfield_registers addr, uint16_t* outputs);

/**
 * Writes to a register with an output-based address offset.
 * To write to:
 * LP5523_CONTROL
 * LP5523_PWM
 * LP5523_CURRENT_CONTROL
 *
 * @param The LED interface.
 * @param The register address to write to (the MSB register).
 * @param Output ID 1-9.
 * @param Value to write.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_output_reg(struct led_itf *itf,
    enum lp5523_output_registers addr, uint8_t output, uint8_t value);

/**
 * Reads from a register with an output-based address offset.
 * To read from:
 * LP5523_CONTROL
 * LP5523_PWM
 * LP5523_CURRENT_CONTROL
 *
 * @param The sensor interface.
 * @param The register address to read from (the MSB register).
 * @param Output ID 1-9.
 * @param Pointer to where the register values should be written.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_get_output_reg(struct led_itf *itf,
    enum lp5523_output_registers addr, uint8_t output, uint8_t *value);

/**
 * Writes to a register with an engine-based address offset.
 * To write to:
 * LP5523_ENGINE_PC
 * LP5523_ENG_PROG_START_ADDR
 * LP5523_MASTER_FADER
 *
 * @param The sensor interface.
 * @param The register address to write to (the MSB register).
 * @param Engine ID 1-3.
 * @param Value to write.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_engine_reg(struct led_itf *itf,
    enum lp5523_engine_registers addr, uint8_t engine, uint8_t value);

/**
 * Reads from a register with an engine-based address offset.
 * To read from:
 * LP5523_ENGINE_PC
 * LP5523_ENG_PROG_START_ADDR
 * LP5523_MASTER_FADER
 *
 * @param The sensor interface.
 * @param The register address to read from (the MSB register).
 * @param Engine ID 1-3.
 * @param Pointer to where the register values should be written.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_get_engine_reg(struct led_itf *itf,
    enum lp5523_engine_registers addr, uint8_t engine, uint8_t *value);

/**
 * Sets the CHIP_EN bit in the ENABLE register.
 *
 * @param The sensor interface.
 * @param Non 0 to enable the device, 0 to disable it.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_enable(struct led_itf *itf, uint8_t enable);

/**
 * Sets the ENGINEX_MODE and ENGINEX_EXEC bits in the ENGINE CNTRLX registers.
 *
 * @param The sensor interface struct.
 * @param Control register to update: LP5523_ENGINE_CNTRL1 or
 *  LP5523_ENGINE_CNTRL2.
 * @param Engines to update, a combination of LP5523_ENGINE3_MASK,
 *  LP5523_ENGINE2_MASK or LP5523_ENGINE1_MASK.
 * @param Values to write, the lower 6 bits are relavant, 2 per engine.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_engine_control(struct led_itf *itf,
    enum lp5523_engine_control_registers addr, uint8_t engine_mask,
    uint8_t values);

/**
 * Sets the MAPPING bits in the relevant DX CONTROL register.
 *
 * @param The sensor interface struct.
 * @param Output number 1-9.
 * @param Mapping 0-3.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_output_mapping(struct led_itf *itf, uint8_t output,
    uint8_t mapping);

/**
 * Gets the MAPPING bits from the relevant DX CONTROL register.
 *
 * @param The sensor interface struct.
 * @param Output number 1-9.
 * @param Pointer to where the output mapping (0-3) should be written.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_get_output_mapping(struct led_itf *itf, uint8_t output,
    uint8_t *mapping);

/**
 * Sets the LOG_EN bit in the relevant DX CONTROL register.
 *
 * @param The sensor interface struct.
 * @param Output number 1-9.
 * @param Non 0 to enable logarithmic dimming, 0 to disable it.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_output_log_dim(struct led_itf *itf, uint8_t output,
    uint8_t enable);

/**
 * Gets the LOG_EN bit grom the relevant DX CONTROL register.
 *
 * @param The sensor interface struct.
 * @param Output number 1-9.
 * @param Written to 1 if logarithmic dimming is enabled, 0 if it is disabled.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_get_output_log_dim(struct led_itf *itf, uint8_t output,
    uint8_t *enable);

/**
 * Sets the TEMP_COMP bits in the relevant DX CONTROL register.
 *
 * @param The sensor interface struct.
 * @param Output number 1-9.
 * @param Temperature compensation value.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_output_temp_comp(struct led_itf *itf, uint8_t output,
    uint8_t value);

/**
 * Gets the TEMP_COMP bits from the relevant DX CONTROL register.
 *
 * @param The sensor interface struct.
 * @param Output number 1-9.
 * @param Pointer to where the temperature compensation value should be written.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_get_output_temp_comp(struct led_itf *itf, uint8_t output,
    uint8_t *value);

/**
 * Gets the relevant ENGX_INT bit from the status register.
 *
 * @param The sensor interface struct.
 * @param Engine number 1-3.
 * @param Written to the value of the bit.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_get_engine_int(struct led_itf *itf, uint8_t engine,
    uint8_t *flag);

/**
 * Resets the device.
 *
 * @param The sensor interface.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_reset(struct led_itf *itf);

/**
 * Sets the page used for program memory reads and writes.
 *
 * @param The sensor interface struct.
 * @param The page 0-5.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_page_sel(struct led_itf *itf, uint8_t page);

/**
 * Sets or clears the relevant DX bit in the ENGX MAPPING registers.
 *
 * @param The sensor interface.
 * @param ID of the engine 1-3.
 * @param Output number 1-9.
 * @param Non 0 to map the output to the specified engine, 0 to unmap it.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_engine_mapping(struct led_itf *itf, uint8_t engine,
    uint16_t output);

/**
 * Gets the relevant DX bit from the ENGX MAPPING registers.
 *
 * @param The sensor interface.
 * @param ID of the engine 1-3.
 * @param Output number 1-9.
 * @param Written to 1 if the output is mapped to the specified engine, 0 if it
 * is not mapped to that engine.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_get_engine_mapping(struct led_itf *itf, uint8_t engine,
    uint16_t *output);

/**
 * Sets an instruction in program memory.
 *
 * @param The sensor interface struct.
 * @param Address to write to.
 * @param Instruction to write.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_instruction(struct led_itf *itf, uint8_t addr, uint16_t ins);

/**
 * gets an instruction from program memory.
 *
 * @param The sensor interface struct.
 * @param Address to read from.
 * @param Written to the instruction.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_get_instruction(struct led_itf *itf, uint8_t addr, uint16_t *ins);

/**
 * Writes a program to memory.
 *
 * @param The sensor interface struct.
 * @param Engines to reset, a combination of LP5523_ENGINE3, LP5523_ENGINE2
 *  or LP5523_ENGINE1.
 * @param Program to write.
 * @param Address to start writing the program.
 * @param Write size.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_set_program(struct led_itf *itf, uint8_t engines, uint16_t *pgm,
    uint8_t start, uint8_t size);

/**
 * Reads a program from memory.
 *
 * @param The sensor interface struct.
 * @param Buffer to read into.
 * @param Address to start reading the program.
 * @param read size.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_get_program(struct led_itf *itf, uint16_t *pgm, uint8_t start,
    uint8_t size);

/**
 * Reads a program from memory and compares it to one passed in as an arg.
 *
 * @param The sensor interface struct.
 * @param Program to verify against.
 * @param Address to start reading the program.
 * @param read size.
 *
 * @return 0 on success, 1 on verify failure, other non-zero error on other
 * failure.
 */
int lp5523_verify_program(struct led_itf *itf, uint16_t *pgm, uint8_t start,
    uint8_t size);

/**
 * Sets the specified engines to execute their respective programs.
 *
 * @param The sensor interface struct.
 * @param Engines to run, a combination of LP5523_ENGINE3, LP5523_ENGINE2
 *  or LP5523_ENGINE1.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_engines_run(struct led_itf *itf, uint8_t engines);

/**
 * Sets the specified engines to hold execution of their respective programs.
 *
 * @param The sensor interface struct.
 * @param Engines to run, a combination of LP5523_ENGINE3, LP5523_ENGINE2
 *  or LP5523_ENGINE1.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_engines_hold(struct led_itf *itf, uint8_t engines);

/**
 * Sets the specified engines to execute the next instruction of their
 * respective programs and increment their PCs.
 *
 * @param The sensor interface struct.
 * @param Engines to run, a combination of LP5523_ENGINE3, LP5523_ENGINE2
 *  or LP5523_ENGINE1.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_engines_step(struct led_itf *itf, uint8_t engines);

/**
 * Disables the specified engines.
 *
 * @param The sensor interface struct.
 * @param Engines to run, a combination of LP5523_ENGINE3, LP5523_ENGINE2
 *  or LP5523_ENGINE1.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_engines_disable(struct led_itf *itf, uint8_t engines);

/**
 * Reads the ADC on a pin.
 *
 * @param The sensor interface struct.
 * @param Pin to read.
 * @param Written to the voltage.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_read_adc(struct led_itf *itf, uint8_t pin, uint8_t *v);

/**
 * Tests the Device, checks for a clock if necessary and checks the ADC values
 * for each LED pin are reasonable.
 *
 * @param The sensor interface struct.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_self_test(struct led_itf *itf);

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this LED driver.
 * @param Argument passed to OS device init, unused.
 *
 * @return 0 on success, non-zero error on failure.
 */
int lp5523_init(struct os_dev *dev, void *arg);
int lp5523_config(struct lp5523 *lp, struct lp5523_cfg *cfg);

/**
 * Calculate Temp comp bits from correction factor
 *
 * @param corr_factor Correction factor -1.5 < corr_factor < +1.5
 * @param temp_comp Temperature compensation bits 5 bits 11111 - 01111
 *
 * @return 0 on success, non-zero on failure
 */
int
lp5523_calc_temp_comp(float corr_factor, uint8_t *temp_comp);

/**
 * Set output ON for particular output
 *
 * @param itf Ptr to LED itf
 * @param output Number of the output
 * @param on Output 1-ON/0-OFF
 *
 * @return 0 on success, non-zero on failure
 */
int
lp5523_set_output_on(struct led_itf *itf, uint8_t output, uint8_t on);

/**
 * Get output ON for particular output
 *
 * @param itf Ptr to LED itf
 * @param output Number of the output
 * @param on Ptr to output variable Output 1-ON/0-OFF
 *
 * @return 0 on success, non-zero on failure
 */
int
lp5523_get_output_on(struct led_itf *itf, uint8_t output, uint8_t *on);

/**
 *
 * Get status
 *
 * @param LED interface
 * @param Ptr to status
 *
 * @return 0 on success, non-zero on failure
 */
int
lp5523_get_status(struct led_itf *itf, uint8_t *status);

/**
 * Get Output Current control
 *
 * @param itf LED interface
 * @param output Number of the output
 * @param curr_ctrl Ptr to Current control
 *
 * @return 0 on success, non-zero on failure
 */
int
lp5523_get_output_curr_ctrl(struct led_itf *itf, uint8_t output, uint8_t *curr_ctrl);

/**
 *
 * Set output Current control
 *
 * @param itf LED interface
 * @param output Number of the output
 * @param curr_ctrl Current control value to set
 */
int
lp5523_set_output_curr_ctrl(struct led_itf *itf, uint8_t output, uint8_t curr_ctrl);

#if MYNEWT_VAL(LP5523_CLI)
int lp5523_shell_init(void);
#endif

/* instructions
 *
 * programs look like:
 *
 * uint16_t pgm[] = {
 *   LP5523_INS_RAMP_IM(1, 1, 0, 100),
 *   LP5523_INS_SET_PWM_IM(50),
 *   ...
 *   LP5523_INS_MUX_MAP_ADDR(24)
 * };
 *
 * loaded with:
 *
 * lp5523_program_load(sensor, engine, pgm, sizeof(pgm)/sizeof(*pgm));
 */

/* sign */

#define LP5523_POS (0)
#define LP5523_NEG (1)

/* prescaler */

#define LP5523_PS (1)
#define LP5523_NPS (0)

/* mux LEDs */

#define LP5523_MUX_LED1 (1)
#define LP5523_MUX_LED2 (2)
#define LP5523_MUX_LED3 (3)
#define LP5523_MUX_LED4 (4)
#define LP5523_MUX_LED5 (5)
#define LP5523_MUX_LED6 (6)
#define LP5523_MUX_LED7 (7)
#define LP5523_MUX_LED8 (8)
#define LP5523_MUX_LED9 (9)
#define LP5523_MUX_GPO (16)

/* map LEDs */

#define LP5523_MAP_LED1 (0x0001)
#define LP5523_MAP_LED2 (0x0002)
#define LP5523_MAP_LED3 (0x0004)
#define LP5523_MAP_LED4 (0x0008)
#define LP5523_MAP_LED5 (0x0010)
#define LP5523_MAP_LED6 (0x0020)
#define LP5523_MAP_LED7 (0x0040)
#define LP5523_MAP_LED8 (0x0080)
#define LP5523_MAP_LED9 (0x0100)
#define LP5523_MAP_GPO (0x8000)

#define LP5523_INS_PARAM(ins, name, param) \
    ((param << LP5523_INS_ ## ins ## _ ## name ## _POS) & \
    LP5523_INS_ ## ins ## _ ## name ## _MASK)

/*
 * Generates a PWM ramp starting at the current value. At each step the output
 * is incremented or decremented.
 *
 * prescale:
 *      0 = 0.488ms cycle time.
 *      1 = 15.625ms cycle time.
 *
 * step_time: Number of cycles per increment (1-31).
 *
 * sign:
 *      0 = Increment.
 *      1 = Decrement.
 *
 * noi: Number of increments (0-255).
 */

#define LP5523_INS_RAMP_IM_PRESCALE_POS (14)
#define LP5523_INS_RAMP_IM_PRESCALE_MASK (0x4000)
#define LP5523_INS_RAMP_IM_STEP_TIME_POS (9)
#define LP5523_INS_RAMP_IM_STEP_TIME_MASK (0x3e00)
#define LP5523_INS_RAMP_IM_SIGN_POS (8)
#define LP5523_INS_RAMP_IM_SIGN_MASK (0x0100)
#define LP5523_INS_RAMP_IM_NOI_POS (0)
#define LP5523_INS_RAMP_IM_NOI_MASK (0x00ff)

#define LP5523_INS_RAMP_IM(prescale, step_time, sign, noi) \
    (0x0000 | \
    LP5523_INS_PARAM(RAMP_IM, PRESCALE, prescale) | \
    LP5523_INS_PARAM(RAMP_IM, STEP_TIME, step_time) | \
    LP5523_INS_PARAM(RAMP_IM, SIGN, sign) | \
    LP5523_INS_PARAM(RAMP_IM, NOI, noi))

/*
 * Generates a PWM ramp starting at the current value. At each step the output
 * is incremented or decremented.
 *
 * prescale:
 *      0 = 0.488ms cycle time.
 *      1 = 15.625ms cycle time.
 *
 * sign:
 *      0 = Increment.
 *      1 = Decrement.
 *
 * step_time: Pointer to a variable containing the number of cycles per
 * increment.
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 *      The registers value should be in the range 1-31
 *
 * noi: Pointer to a variable containing the number of increments.
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 */

#define LP5523_INS_RAMP_PRESCALE_POS (5)
#define LP5523_INS_RAMP_PRESCALE_MASK (0x0020)
#define LP5523_INS_RAMP_SIGN_POS (4)
#define LP5523_INS_RAMP_SIGN_MASK (0x0010)
#define LP5523_INS_RAMP_STEP_TIME_POS (2)
#define LP5523_INS_RAMP_STEP_TIME_MASK (0x000c)
#define LP5523_INS_RAMP_NOI_POS (0)
#define LP5523_INS_RAMP_NOI_MASK (0x0003)

#define LP5523_INS_RAMP(prescale, sign, step_time, noi) \
    (0x8400 | \
    LP5523_INS_PARAM(RAMP, PRESCALE, prescale) | \
    LP5523_INS_PARAM(RAMP, SIGN, sign) | \
    LP5523_INS_PARAM(RAMP, STEP_TIME, step_time) | \
    LP5523_INS_PARAM(RAMP, NOI, noi))

/*
 * Sets the PWM value for an output.
 *
 * pwm: The PWM value (0-255).
 */

#define LP5523_INS_SET_PWM_IM_PWM_POS (0)
#define LP5523_INS_SET_PWM_IM_PWM_MASK (0x00ff)

#define LP5523_INS_SET_PWM_IM(pwm) \
    (0x4000 | \
    LP5523_INS_PARAM(SET_PWM_IM, PWM, pwm))

/*
 * Sets the PWM value for an output.
 *
 * pwm: Pointer to a variable containing the PWM value.
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 */

#define LP5523_INS_SET_PWM_PWM_POS (0)
#define LP5523_INS_SET_PWM_PWM_MASK (0x0003)

#define LP5523_INS_SET_PWM(pwm) \
    (0x8460 | \
    LP5523_INS_PARAM(SET_PWM, PWM, pwm))

/*
 * Waits a given number of steps.
 *
 * prescale:
 *      0 = 0.488ms cycle time.
 *      1 = 15.625ms cycle time.
 *
 * step_time: Number of cycles per increment (1-31).
 */

#define LP5523_INS_WAIT(prescale, step_time) \
    LP5523_INS_RAMP_IM(prescale, step_time, 0, 0)

#define LP5523_INS_MUX_PAR_POS (0)
#define LP5523_INS_MUX_PAR_MASK (0x007f)

#define LP5523_INS_MUX(opcode, par) \
    (opcode | \
    LP5523_INS_PARAM(MUX, PAR, par))

/*
 * Defines the address of the start of the mapping table.
 *
 * addr: The address of the start of the mapping table (0-95).
 */
#define LP5523_INS_MUX_LD_START(addr) \
    LP5523_INS_MUX(0x9e00, addr)

/*
 * Defines the address of the start of the mapping table and activates the first
 * row.
 *
 * addr: The address of the start of the mapping table (0-95).
 */
#define LP5523_INS_MUX_MAP_START(addr) \
    LP5523_INS_MUX(0x9c00, addr)

/*
 * Defines the address of the end of the mapping table.
 *
 * addr: The address of the end of the mapping table (0-95).
 */
#define LP5523_INS_MUX_LD_END(addr) \
    LP5523_INS_MUX(0x9c80, addr)

/*
 * Connects one LED driver to the execution engine.
 *
 * sel: The LED to connect (0-16).
 *      0 = no drivers
 *      1-9 = LEDs.
 *      16 = GPO.
 */
#define LP5523_INS_MUX_SEL(sel) \
    LP5523_INS_MUX(0x9d00, sel)

/*
 * Clears engine to driver mapping.
 */
#define LP5523_INS_MUX_CLR() \
    LP5523_INS_MUX_SEL(0)

/*
 * Increments the index pointer and sets that row active. Wraps to the start
 * of the table if the end of the table is reached.
 */
#define LP5523_INS_MUX_MAP_NEXT() \
    (0x9d80)

/*
 * Decrements the index pointer and sets that row active. Wraps to the end
 * of the table if the start of the table is reached.
 */
#define LP5523_INS_MUX_MAP_PREV() \
    (0x9dc0)

/*
 * Increments the index pointer. Wraps to the start of the table if the end
 * of the table is reached.
 */
#define LP5523_INS_MUX_LD_NEXT() \
    (0x9d81)

/*
 * Decrements the index pointer. Wraps to the end of the table if the start
 * of the table is reached.
 */
#define LP5523_INS_MUX_LD_PREV() \
    (0x9dc1)

/*
 * Sets the index pointer to point to a table row.
 *
 * addr: The absolute address of the table row (0-95).
 */
#define LP5523_INS_MUX_LD_ADDR(addr) \
    LP5523_INS_MUX(0x9f00, addr)

/*
 * Sets the index pointer to point to a table row and sets that row active.
 *
 * addr: The absolute address of the table row (0-95).
 */
#define LP5523_INS_MUX_MAP_ADDR(addr) \
    LP5523_INS_MUX(0x9f80, addr)

/*
 * Reset
 */
#define LP5523_INS_RST() \
    (0x0000)

/*
 * Branches to an absolute address.
 *
 * step_number: The absolute address to branch to (0-95).
 *
 * loop_count: The number of times to branch, 0 indicates infintity.
 */
#define LP5523_INS_BRANCH_IM_LOOP_COUNT_POS (7)
#define LP5523_INS_BRANCH_IM_LOOP_COUNT_MASK (0x01f80)
#define LP5523_INS_BRANCH_IM_STEP_NUMBER_POS (0)
#define LP5523_INS_BRANCH_IM_STEP_NUMBER_MASK (0x007f)

#define LP5523_INS_BRANCH_IM(loop_count, step_number) \
    (0xa000 | \
    LP5523_INS_PARAM(BRANCH_IM, LOOP_COUNT, loop_count) | \
    LP5523_INS_PARAM(BRANCH_IM, STEP_NUMBER, step_number))

/*
 * Branches to an absolute address.
 *
 * step_number: The absolute address to branch to (0-95).
 *
 * loop_count: Pointer to a variable containing the number of loops.
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 */
#define LP5523_INS_BRANCH_STEP_NUMBER_POS (2)
#define LP5523_INS_BRANCH_STEP_NUMBER_MASK (0x01fc)
#define LP5523_INS_BRANCH_LOOP_COUNT_POS (0)
#define LP5523_INS_BRANCH_LOOP_COUNT_MASK (0x0003)

#define LP5523_INS_BRANCH(step_number, loop_count) \
    (0x8600 | \
    LP5523_INS_PARAM(BRANCH, STEP_NUMBER, step_number) | \
    LP5523_INS_PARAM(BRANCH, LOOP_COUNT, loop_count))

#define LP5523_INS_INT() \
    (0xc400)

/*
 * Ends the program execution.
 */
#define LP5523_INS_END_INTERRUPT_POS (12)
#define LP5523_INS_END_INTERRUPT_MASK (0x1000)
#define LP5523_INS_END_RESET_POS (11)
#define LP5523_INS_END_RESET_MASK (0x0800)

#define LP5523_INS_END(interrupt, reset) \
    (0xc000 | \
    LP5523_INS_PARAM(END, INTERRUPT, interrupt) | \
    LP5523_INS_PARAM(END, RESET, reset))

/*
 * Wait on, or send triggers that can be waited on by or sent from other
 * engines.
 *
 * wait_external: Wait for an external trigger.
 *
 * wait_engines: Wait for a trigger from one of these engines (3 bit value,
 * each bit representing an engine the LSB being engine 1).
 *
 * send_external: Send an external trigger.
 *
 * send_engines: Send a trigger to all of these engines (3 bit value,
 * each bit representing an engine the LSB being engine 1).
 */
#define LP5523_INS_TRIGGER_WAIT_EXTERNAL_POS (12)
#define LP5523_INS_TRIGGER_WAIT_EXTERNAL_MASK (0x1000)
#define LP5523_INS_TRIGGER_WAIT_ENGINES_POS (7)
#define LP5523_INS_TRIGGER_WAIT_ENGINES_MASK (0x0310)
#define LP5523_INS_TRIGGER_SEND_EXTERNAL_POS (6)
#define LP5523_INS_TRIGGER_SEND_EXTERNAL_MASK (0x0040)
#define LP5523_INS_TRIGGER_SEND_ENGINES_POS (1)
#define LP5523_INS_TRIGGER_SEND_ENGINES_MASK (0x000e)

#define LP5523_INS_TRIGGER(wait_external, wait_engines, send_external, \
    send_engines) \
    (0xe000 | \
    LP5523_INS_PARAM(TRIGGER, WAIT_EXTERNAL, wait_external) | \
    LP5523_INS_PARAM(TRIGGER, WAIT_ENGINES, wait_engines) | \
    LP5523_INS_PARAM(TRIGGER, SEND_EXTERNAL, send_external) | \
    LP5523_INS_PARAM(TRIGGER, SEND_ENGINES, send_engines))

#define LP5523_INS_J_SKIP_POS (4)
#define LP5523_INS_J_SKIP_MASK (0x01f0)
#define LP5523_INS_J_VARIABLE1_POS (2)
#define LP5523_INS_J_VARIABLE1_MASK (0x000c)
#define LP5523_INS_J_VARIABLE2_POS (0)
#define LP5523_INS_J_VARIABLE2_MASK (0x0003)

#define LP5523_INS_J(opcode, skip, variable1, variable2) \
    (opcode | \
    LP5523_INS_PARAM(J, SKIP, skip) | \
    LP5523_INS_PARAM(J, VARIABLE1, variable1) | \
    LP5523_INS_PARAM(J, VARIABLE2, variable2))

/*
 * Skip if not equal.
 *
 * skip: Number of operations to be skipped if not equal (0-31).
 *
 * variable1: Compared to variable2 (0-3).
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 *
 * variable2: Compared to variable1 (0-3).
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 */
#define LP5523_INS_JNE(skip, variable1, variable2) \
    LP5523_INS_J(0x8800, skip, variable1, variable2)

/*
 * Skip if less than.
 *
 * skip: Number of operations to be skipped if less (0-31).
 *
 * variable1: Compared to variable2 (0-3).
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 *
 * variable2: Compared to variable1 (0-3).
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 */
#define LP5523_INS_JL(skip, variable1, variable2) \
    LP5523_INS_J(0x8a00, skip, variable1, variable2)

/*
 * Skip if greater or equal than.
 *
 * skip: Number of operations to be skipped if greater or equal (0-31).
 *
 * variable1: Compared to variable2 (0-3).
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 *
 * variable2: Compared to variable1 (0-3).
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 */
#define LP5523_INS_JGE(skip, variable1, variable2) \
    LP5523_INS_J(0x8c00, skip, variable1, variable2)

/*
 * Skip if equal.
 *
 * skip: Number of operations to be skipped if equal (0-31).
 *
 * variable1: Compared to variable2 (0-3).
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 *
 * variable2: Compared to variable1 (0-3).
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 */

#define LP5523_INS_JE(skip, variable1, variable2) \
    LP5523_INS_J(0x8e00, skip, variable1, variable2)

#define LP5523_INS_ARITH_TARGET_VARIABLE_POS (10)
#define LP5523_INS_ARITH_TARGET_VARIABLE_MASK (0x0c00)

#define LP5523_INS_ARITH_IM_VALUE_POS (0)
#define LP5523_INS_ARITH_IM_VALUE_MASK (0x00ff)

#define LP5523_INS_ARITH_IM(opcode, target_variable, value) \
    (opcode | \
    LP5523_INS_PARAM(ARITH, TARGET_VARIABLE, target_variable) | \
    LP5523_INS_PARAM(ARITH_IM, VALUE, value))

#define LP5523_INS_ARITH_VARIABLE1_POS (2)
#define LP5523_INS_ARITH_VARIABLE1_MASK (0x000c)
#define LP5523_INS_ARITH_VARIABLE2_POS (0)
#define LP5523_INS_ARITH_VARIABLE2_MASK (0x0003)

#define LP5523_INS_ARITH(opcode, target_variable, variable1, variable2) \
    (opcode | \
    LP5523_INS_PARAM(ARITH, TARGET_VARIABLE, target_variable) | \
    LP5523_INS_PARAM(ARITH, VARIABLE1, variable1) | \
    LP5523_INS_PARAM(ARITH, VARIABLE2, variable2))

/*
 * Load a value into a variable.
 *
 * target_variable: (0-2)
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *
 * value:
 */
#define LP5523_INS_LD(target_variable, value) \
    LP5523_INS_ARITH_IM(0x9000, target_variable, value)

/*
 * Add an immediate value to a variable, place the result back in the
 * varible.
 *
 * target_variable: (0-2)
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *
 * value:
 */
#define LP5523_INS_ADD_IM(target_variable, value) \
    LP5523_INS_ARITH_IM(0x9100, target_variable, value)

/*
 * Add one variable to another, place the result in a third target variable.
 *
 * target_variable: (0-2)
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *
 * variable1: Variable containing the augend (0-3).
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 *
 * variable2: Variable containing the addend (0-3).
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 */
#define LP5523_INS_ADD(target_variable, variable1, variable2) \
    LP5523_INS_ARITH(0x9300, target_variable, variable1, variable2)

/*
 * Subtract an immediate value from a variable, place the result back in the
 * varible.
 *
 * target_variable: (0-2)
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *
 * value:
 */
#define LP5523_INS_SUB_IM(target_variable, value) \
    LP5523_INS_ARITH_IM(0x9200, target_variable, value)

/*
 * Subtract one variable from another, place the result in a third target
 * variable.
 *
 * target_variable: (0-2)
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *
 * variable1: Variable containing the minuend (0-3).
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 *
 * variable2: Variable containing the subtrahend (0-3).
 *      0 = local varible A.
 *      1 = local variable B.
 *      2 = global variable C.
 *      3 = register address 0x3C variable D value or register address 0x42
 *          value.
 */
#define LP5523_INS_SUB(target_variable, variable1, variable2) \
    LP5523_INS_ARITH(0x9310, target_variable, variable1, variable2)

#ifdef __cplusplus
}
#endif

#endif
