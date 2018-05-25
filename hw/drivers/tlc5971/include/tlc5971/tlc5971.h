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
#ifndef __TLC5971_H__
#define __TLC5971_H__

#include "os/mynewt.h"

#define TLC5971_NUM_LED_CHANNELS            4

#define TLC5971_GRAYSCALE_MIN               0
#define TLC5971_GRAYSCALE_MAX               0xFFFFU

#define TLC5971_GLOBAL_BRIGHTNESS_MIN       0
#define TLC5971_GLOBAL_BRIGHTNESS_MAX       0x7FU

#define TLC5971_PACKET_LENGTH               28

#define TLC5971_WRITE_COMMAND               0x25

#define TLC5971_DATA_BYTE27(ctrl)           \
    (TLC5971_WRITE_COMMAND << 2 | (ctrl & (TLC5971_CTRL_MASK_LOW)))

#define TLC5971_DATA_BYTE26(ctrl, bc_blu)   \
    ((ctrl & TLC5971_CTRL_MASK_HIGH) |      \
    (bc_blu >> 2 & TLC5971_BC_BLU_BYTE_26_MASK))

#define TLC5971_DATA_BYTE25(bc_blu, bc_grn) \
    (((bc_blu << 6) & TLC5971_BC_BLU_BYTE_25_MASK) | \
    ((bc_grn >> 1) & TLC5971_BC_GRN_BYTE_25_MASK))

#define TLC5971_DATA_BYTE24(bc_grn, bc_red) \
    (((bc_grn << 7) & TLC5971_BC_GRN_BYTE_24_MASK) | (bc_red))

#define TLC5971_DATA_GS_H(gs_value)         (gs_value >> 8)

/* Bit definition helpers */
#define BIT0    (1 << 0)
#define BIT1    (1 << 1)
#define BIT2    (1 << 2)
#define BIT3    (1 << 3)
#define BIT4    (1 << 4)
#define BIT5    (1 << 5)
#define BIT6    (1 << 6)
#define BIT7    (1 << 7)

/*-------------------------TYPEDEFS AND STRUCTURES--------------------------*/
typedef enum
{
    TLC5971_CHANNEL_0,
    TLC5971_CHANNEL_1,
    TLC5971_CHANNEL_2,
    TLC5971_CHANNEL_3,
    TLC5971_ALL_CHANNELS
} tlc5971_channel_t;

typedef enum
{
    TLC5971_BC_CHANNEL_RED,
    TLC5971_BC_CHANNEL_GREEN,
    TLC5971_BC_CHANNEL_BLUE,
    TLC5971_BC_CHANNEL_ALL
} tlc5971_bc_channel_t;

typedef enum
{
    TLC5971_BLANK_OFFSET                =   5,
    TLC5971_DSPRPT_OFFSET               =   6,
    TLC5971_TMGRST_OFFSET               =   7,
    TLC5971_EXTGCK_OFFSET               =   0,
    TLC5971_OUTTMG_OFFSET               =   1
} tlc5971_control_bit_offset_t;

typedef enum
{
    TLC5971_BC_BLU_BYTE_26_MASK         =   BIT4 | BIT3 | BIT2 | BIT1 | BIT0,
    TLC5971_BC_BLU_BYTE_25_MASK         =   BIT7 | BIT6,
    TLC5971_BC_GRN_BYTE_25_MASK         =   BIT5 | BIT4 | BIT3 | \
                                            BIT2 | BIT1 | BIT0,
    TLC5971_BC_GRN_BYTE_24_MASK         =   BIT7
} tlc5971_bc_packet_mask_t;

typedef enum
{
    TLC5971_BLANK_MASK                  =   BIT5,
    TLC5971_DSPRPT_MASK                 =   BIT6,
    TLC5971_TMGRST_MASK                 =   BIT7,
    TLC5971_EXTGCK_MASK                 =   BIT0,
    TLC5971_OUTTMG_MASK                 =   BIT1,
    TLC5971_CTRL_MASK_HIGH              =   TLC5971_BLANK_MASK | \
                                            TLC5971_DSPRPT_MASK | \
                                            TLC5971_TMGRST_MASK,
    TLC5971_CTRL_MASK_LOW               =   TLC5971_EXTGCK_MASK | \
                                            TLC5971_OUTTMG_MASK,
    TLC5971_MASK_ALL                    =   TLC5971_BLANK_MASK | \
                                            TLC5971_DSPRPT_MASK | \
                                            TLC5971_TMGRST_MASK | \
                                            TLC5971_EXTGCK_MASK | \
                                            TLC5971_OUTTMG_MASK
} tlc5971_control_data_mask_t;

typedef struct
{
    uint8_t bc_red;
    uint8_t bc_green;
    uint8_t bc_blue;
} tlc5971_global_brightness_t;

typedef struct
{
    uint16_t gs_red;
    uint16_t gs_green;
    uint16_t gs_blue;
} tlc5971_grayscale_control_t;

/*
 * TLC5971 Configuration
 *
 * The configuration consists of one byte of control data. The control
 * data represents bits 217 to 213 in the data latch. The masks are bit
 * positions in the bytes that get sent to the device.
 */

/* These helper macros are used to set/clear config masks in the control data */
#define TLC5971_BLANK_LEDS(cfg) \
    ((cfg)->tlc_ctrl_data |= (uint8_t)TLC5971_BLANK_MASK)

#define TLC5971_UNBLANK_LEDS(cfg) \
    ((cfg)->tlc_ctrl_data &= (uint8_t)~TLC5971_BLANK_MASK)

#define TLC5971_ENABLE_AUTO_REPEAT(cfg) \
    ((cfg)->tlc_ctrl_data |= (uint8_t)TLC5971_DSPRPT_MASK)

#define TLC5971_DISABLE_AUTO_REPEAT(cfg) \
    ((cfg)->tlc_ctrl_data &= (uint8_t)~TLC5971_DSPRPT_MASK)

#define TLC5971_ENABLE_TIMING_RESET(cfg) \
    ((cfg)->tlc_ctrl_data |= (uint8_t)TLC5971_TMGRST_MASK)

#define TLC5971_DISABLE_TIMING_RESET(cfg) \
    ((cfg)->tlc_ctrl_data &= (uint8_t)~TLC5971_TMGRST_MASK)

#define TLC5971_SET_CLOCK_SCK(cfg) \
    ((cfg)->tlc_ctrl_data |= (uint8_t)TLC5971_EXTGCK_MASK)

#define TLC5971_SET_CLOCK_INTERNAL(cfg) \
    ((cfg)->tlc_ctrl_data &= (uint8_t)~TLC5971_EXTGCK_MASK)

#define TLC5971_SET_CE_RISING(cfg) \
    ((cfg)->tlc_ctrl_data |= (uint8_t)TLC5971_OUTTMG_MASK)

#define TLC5971_SET_CE_FALLING(cfg) \
    ((cfg)->tlc_ctrl_data &= (uint8_t)~TLC5971_OUTTMG_MASK)

struct tlc5971_cfg {
    uint8_t tlc_ctrl_data;
};

/*
 * TLC5971 peripheral interface
 *
 * This is the interface structure used to map SPI peripherals to the chip.
 */
struct tlc5971_periph_itf {
    uint8_t     tpi_spi_num;
    uint32_t    tpi_spi_freq;
};

/*
 * TLC5971 Device
 */
struct tlc5971_dev {
    struct os_dev               tlc_dev;    /* Must be 1st member of struct */
    struct tlc5971_periph_itf   tlc_itf;
    tlc5971_grayscale_control_t gs[TLC5971_NUM_LED_CHANNELS];
    tlc5971_global_brightness_t bc;
    uint8_t                     control_data;
    uint8_t                     is_enabled;
    uint8_t                     data_packet[TLC5971_PACKET_LENGTH];
};

int tlc5971_init(struct os_dev *dev, void *arg);
int tlc5971_is_enabled(struct tlc5971_dev *dev);
int tlc5971_write(struct tlc5971_dev *dev);
void tlc5971_set_cfg(struct tlc5971_dev *dev, struct tlc5971_cfg *cfg);
void tlc5971_get_cfg(struct tlc5971_dev *dev, struct tlc5971_cfg *cfg);
void tlc5971_set_global_brightness(struct tlc5971_dev *dev,
                                   tlc5971_bc_channel_t bc_channel,
                                   uint8_t brightness);
void tlc5971_set_channel_rgb(struct tlc5971_dev *dev, tlc5971_channel_t,
                             uint16_t red, uint16_t green, uint16_t blue);

#endif /* __TLC5971_H__ */