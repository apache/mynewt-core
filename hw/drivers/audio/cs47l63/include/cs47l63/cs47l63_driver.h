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

#ifndef _HW_AUDIO_CS47L63_H
#define _HW_AUDIO_CS47L63_H

#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <assert.h>
#include <bus/drivers/spi_hal.h>
#include <bsp_driver_if.h>
#include <cs47l63.h>

#define CS47L63_DEVID_VAL 0x47A63

/* Application does not need to use any of fields directly */
struct cs47l63_dev {
    struct bus_spi_node spi_node;
    /* SDK data */
    cs47l63_t cs47l63;
    atomic_int pending_irq_cnt;
    /* Event for interrupt handling in task */
    struct os_event irq_event;
    /* Volume on out1 output in 0.5 dB, use cs47l63_volume_set to change */
    int16_t out1l_volume_1;
};

struct cs47l63_create_cfg {
    const struct bus_spi_node_cfg spi_cfg;
};

struct reg_val_pair {
    uint32_t reg;
    uint32_t val;
};

/**
 * Write CS47L63 registers
 *
 * @param dev - cs47l63 device
 * @param regs - zero terminated array of registers to write
 * @return 0 on success, non-zero on failure
 */
uint32_t cs47l63_write_regs(struct cs47l63_dev *dev, const struct reg_val_pair regs[]);

/**
 * Read CS47L63 register
 *
 * @param dev - cs47l63 device
 * @param reg - register number
 * @param val - pointer to register value
 * @return 0 on success, non-zero on failure
 */
uint32_t cs47l63_reg_read(struct cs47l63_dev *dev, uint32_t reg, uint32_t *val);

/**
 * Write CS47L63 register
 *
 * @param dev - cs47l63 device
 * @param reg - register number
 * @param val - register value
 * @return 0 on success, non-zero on failure
 */
uint32_t cs47l63_reg_write(struct cs47l63_dev *dev, uint32_t reg, uint32_t val);

/**
 * Create CS47L63 device
 *
 * @param dev - device to create
 * @param name - device name
 * @param cfg - create parameters for
 * @return 0 on success, non-zero on failure
 */
int cs47l63_create_dev(struct cs47l63_dev *dev, const char *name, const struct cs47l63_create_cfg *cfg);

/**
 * Enable or disable FLL of CS47L63
 *
 * @param dev - CS47L63 device
 * @param enable - true to turn on FLL, false to turn it off
 * @return 0 on success, non-zero on failure
 */
uint32_t cs47l63_fll_control(struct cs47l63_dev *dev, bool enable);

/**
 * Start I2S interface of CS47L63
 *
 * @param dev - cs47l63 device
 * @param sample_rate - sample rate for I2S
 * @param slot_bits - number of bits per single channel sample
 * @return 0 on success, non-zero on failure
 */
int cs47l63_start_i2s(struct cs47l63_dev *dev, uint32_t sample_rate, uint32_t slot_bits);

/**
 * Get volume level of out1 in dB
 *
 * @param dev - cs47l63 device
 * @param vol - current volume value of out1
 * @return 0 on success, non-zero on failure
 */
int cs47l63_volume_get(struct cs47l63_dev *dev, int8_t *vol);

/**
 * Modify output volume by number of dB
 *
 * @param dev     device to modify.
 * @param vol_adj number of decibel to increase of decrease volume.
 * @return 0 on success, non-zero on failure
 */
int cs47l63_volume_modify(struct cs47l63_dev *dev, int8_t vol_adj);

/**
 * Set output volume in dB
 *
 * @param dev device to set volume to
 * @param vol new volume value (-64 dB .. 31 dB)
 * @return 0 on success, non-zero on failure
 */
int cs47l63_volume_set(struct cs47l63_dev *dev, int8_t vol);

/**
 * Mute/unmute audio
 *
 * @param dev  device to mute/unumte
 * @param mute true - mute, false - unmute
 * @return 0 on success, non-zero on failure
 */
uint32_t cs47l63_mute_control(struct cs47l63_dev *dev, bool mute);

#if MYNEWT_VAL(CS47L63_0)
extern struct cs47l63_dev *cs47l63_0;
#endif

#endif
