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

#ifndef __BMA400_PRIV_H__
#define __BMA400_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

struct bma400;

/*
 * Full register map:
 */

#define BMA400_REG_CHIPID                           0x00
#define BMA400_REG_ERR_REG                          0x02
#define BMA400_REG_STATUS                           0x03
#define BMA400_REG_ACC_X_LSB                        0x04
#define BMA400_REG_ACC_X_MSB                        0x05
#define BMA400_REG_ACC_Y_LSB                        0x06
#define BMA400_REG_ACC_Y_MSB                        0x07
#define BMA400_REG_ACC_Z_LSB                        0x08
#define BMA400_REG_ACC_Z_MSB                        0x09
#define BMA400_REG_SENSOR_TIME0                     0x0a
#define BMA400_REG_SENSOR_TIME1                     0x0b
#define BMA400_REG_SENSOR_TIME2                     0x0c
#define BMA400_REG_EVENT                            0x0d
#define BMA400_REG_INT_STAT0                        0x0e
#define BMA400_REG_INT_STAT1                        0x0f
#define BMA400_REG_INT_STAT2                        0x10
#define BMA400_REG_TEMP_DATA                        0x11
#define BMA400_REG_FIFO_LENGTH0                     0x12
#define BMA400_REG_FIFO_LENGTH1                     0x13
#define BMA400_REG_FIFO_DATA                        0x14
#define BMA400_REG_STEP_CNT_0                       0x15
#define BMA400_REG_STEP_CNT_1                       0x16
#define BMA400_REG_STEP_CNT_2                       0x17
#define BMA400_REG_STEP_STAT                        0x18

#define BMA400_REG_ACC_CONFIG0                      0x19
#define BMA400_REG_ACC_CONFIG1                      0x1a
#define BMA400_REG_ACC_CONFIG2                      0x1b
#define BMA400_REG_INT_CONFIG0                      0x1f
#define BMA400_REG_INT_CONFIG1                      0x20
#define BMA400_REG_INT1_MAP                         0x21
#define BMA400_REG_INT2_MAP                         0x22
#define BMA400_REG_INT12_MAP                        0x23
#define BMA400_REG_INT12_IO_CTRL                    0x24

#define BMA400_REG_FIFO_CONFIG0                     0x26
#define BMA400_REG_FIFO_CONFIG1                     0x27
#define BMA400_REG_FIFO_CONFIG2                     0x28
#define BMA400_REG_FIFO_PWR_CONFIG                  0x29
#define BMA400_REG_AUTOLOWPOW_0                     0x2a
#define BMA400_REG_AUTOLOWPOW_1                     0x2b
#define BMA400_REG_AUTOWAKEUP_0                     0x2c
#define BMA400_REG_AUTOWAKEUP_1                     0x2d

#define BMA400_REG_WKUP_INT_CONFIG0                 0x2f
#define BMA400_REG_WKUP_INT_CONFIG1                 0x30
#define BMA400_REG_WKUP_INT_CONFIG2                 0x31
#define BMA400_REG_WKUP_INT_CONFIG3                 0x32
#define BMA400_REG_WKUP_INT_CONFIG4                 0x33

#define BMA400_REG_ORIENTCH_CONFIG0                 0x35
#define BMA400_REG_ORIENTCH_CONFIG1                 0x36

#define BMA400_REG_ORIENTCH_CONFIG3                 0x38
#define BMA400_REG_ORIENTCH_CONFIG4                 0x39
#define BMA400_REG_ORIENTCH_CONFIG5                 0x3a
#define BMA400_REG_ORIENTCH_CONFIG6                 0x3b
#define BMA400_REG_ORIENTCH_CONFIG7                 0x3c
#define BMA400_REG_ORIENTCH_CONFIG8                 0x3d
#define BMA400_REG_ORIENTCH_CONFIG9                 0x3e
#define BMA400_REG_GEN1INT_CONFIG0                  0x3f
#define BMA400_REG_GEN1INT_CONFIG1                  0x40
#define BMA400_REG_GEN1INT_CONFIG2                  0x41
#define BMA400_REG_GEN1INT_CONFIG3                  0x42
#define BMA400_REG_GEN1INT_CONFIG31                 0x43
#define BMA400_REG_GEN1INT_CONFIG4                  0x44
#define BMA400_REG_GEN1INT_CONFIG5                  0x45
#define BMA400_REG_GEN1INT_CONFIG6                  0x46
#define BMA400_REG_GEN1INT_CONFIG7                  0x47
#define BMA400_REG_GEN1INT_CONFIG8                  0x48
#define BMA400_REG_GEN1INT_CONFIG9                  0x49
#define BMA400_REG_GEN2INT_CONFIG0                  0x4a
#define BMA400_REG_GEN2INT_CONFIG1                  0x4b
#define BMA400_REG_GEN2INT_CONFIG2                  0x4c
#define BMA400_REG_GEN2INT_CONFIG3                  0x4d
#define BMA400_REG_GEN2INT_CONFIG31                 0x4e
#define BMA400_REG_GEN2INT_CONFIG4                  0x4f
#define BMA400_REG_GEN2INT_CONFIG5                  0x50
#define BMA400_REG_GEN2INT_CONFIG6                  0x51
#define BMA400_REG_GEN2INT_CONFIG7                  0x52
#define BMA400_REG_GEN2INT_CONFIG8                  0x53
#define BMA400_REG_GEN2INT_CONFIG9                  0x54
#define BMA400_REG_ACTCH_CONFIG0                    0x55
#define BMA400_REG_ACTCH_CONFIG1                    0x56
#define BMA400_REG_TAP_CONFIG                       0x57
#define BMA400_REG_TAP_CONFIG0                      BMA400_REG_TAP_CONFIG
#define BMA400_REG_TAP_CONFIG1                      0x58

#define BMA400_REG_STEP_COUNTER_CONFIG0             0x59
#define BMA400_REG_STEP_COUNTER_CONFIG1             0x5a
#define BMA400_REG_STEP_COUNTER_CONFIG2             0x5b
#define BMA400_REG_STEP_COUNTER_CONFIG3             0x5c
#define BMA400_REG_STEP_COUNTER_CONFIG4             0x5d
#define BMA400_REG_STEP_COUNTER_CONFIG5             0x5e
#define BMA400_REG_STEP_COUNTER_CONFIG6             0x5f
#define BMA400_REG_STEP_COUNTER_CONFIG7             0x60
#define BMA400_REG_STEP_COUNTER_CONFIG8             0x61
#define BMA400_REG_STEP_COUNTER_CONFIG9             0x62
#define BMA400_REG_STEP_COUNTER_CONFIG10            0x63
#define BMA400_REG_STEP_COUNTER_CONFIG11            0x64
#define BMA400_REG_STEP_COUNTER_CONFIG12            0x65
#define BMA400_REG_STEP_COUNTER_CONFIG13            0x66
#define BMA400_REG_STEP_COUNTER_CONFIG14            0x67
#define BMA400_REG_STEP_COUNTER_CONFIG15            0x68
#define BMA400_REG_STEP_COUNTER_CONFIG16            0x69
#define BMA400_REG_STEP_COUNTER_CONFIG17            0x6a
#define BMA400_REG_STEP_COUNTER_CONFIG18            0x6b
#define BMA400_REG_STEP_COUNTER_CONFIG19            0x6c
#define BMA400_REG_STEP_COUNTER_CONFIG20            0x6d
#define BMA400_REG_STEP_COUNTER_CONFIG21            0x6e
#define BMA400_REG_STEP_COUNTER_CONFIG22            0x6f
#define BMA400_REG_STEP_COUNTER_CONFIG23            0x70

#define BMA400_REG_IF_CONF                          0x7c
#define BMA400_REG_SELF_TEST                        0x7d
#define BMA400_REG_CMD                              0x7e

/* Set Read operation bit in SPI communication */
#define BMA400_SPI_READ_CMD_BIT(_reg)               (_reg |= 0x80)

#define BMA400_STATUS_DRDY_STAT                     0x80
#define BMA400_STATUS_CMD_RDY                       0x10
#define BMA400_STATUS_POWER_MODE_STAT               0x06
#define BMA400_STATUS_INT_ACTIVE                    0x01

#define BMA400_STATUS_SLEEP_MODE                    0x00
#define BMA400_STATUS_LOW_POWER_MODE                0x02
#define BMA400_STATUS_NORMAL_MODE                   0x04

#define BMA400_EVENT_POR_DETECTED                   0x01

#define BMA400_INT_STAT0_DRDY_INT_STAT              0x80
#define BMA400_INT_STAT0_FWM_INT_STAT               0x40
#define BMA400_INT_STAT0_FFULL_INT_STAT             0x20
#define BMA400_INT_STAT0_IENG_OVERRUN_STAT          0x10
#define BMA400_INT_STAT0_GEN2_INT_STAT              0x08
#define BMA400_INT_STAT0_GEN1_INT_STAT              0x04
#define BMA400_INT_STAT0_ORIENTCH_INT_STAT          0x02
#define BMA400_INT_STAT0_WKUP_INT_STAT              0x01

#define BMA400_INT_STAT1_IENG_OVERRUN_STAT          0x10
#define BMA400_INT_STAT1_D_TAP_INT_STAT             0x08
#define BMA400_INT_STAT1_S_TAP_INT_STAT             0x04
#define BMA400_INT_STAT1_STEP_INT_STAT              0x03

#define BMA400_INT_STAT1_STEP_DETECTED              0x01
#define BMA400_INT_STAT1_STEP_D_DETECTED            0x02

#define BMA400_INT_STAT2_IENG_OVERRUN_STAT          0x10
#define BMA400_INT_STAT2_ACTCH_Z_INT_STAT           0x04
#define BMA400_INT_STAT2_ACTCH_Y_INT_STAT           0x02
#define BMA400_INT_STAT2_ACTCH_X_INT_STAT           0x01
#define BMA400_INT_STAT2_ACTCH_XYZ_INT_STAT         0x07

#define BMA400_STEP_STAT_STEP_STAT_FIELD            0x03

#define BMA400_ACC_CONFIG0_FILT1_BW                 0x80
#define BMA400_ACC_CONFIG0_OSR_LP                   0x60
#define BMA400_ACC_CONFIG0_POWER_MODE_CONF          0x03

#define BMA400_ACC_CONFIG0_SLEEP_MODE               0x00
#define BMA400_ACC_CONFIG0_LOW_POWER_MODE           0x01
#define BMA400_ACC_CONFIG0_NORMAL_MODE              0x02

#define BMA400_ACC_CONFIG1_ACC_RANGE                0xc0
#define BMA400_ACC_CONFIG1_OSR                      0x30
#define BMA400_ACC_CONFIG1_ACC_ODR                  0x0f

#define BMA400_ACC_CONFIG1_ODR_12_5_HZ              0x05
#define BMA400_ACC_CONFIG1_ODR_25_HZ                0x06
#define BMA400_ACC_CONFIG1_ODR_50_HZ                0x07
#define BMA400_ACC_CONFIG1_ODR_100_HZ               0x08
#define BMA400_ACC_CONFIG1_ODR_200_HZ               0x09
#define BMA400_ACC_CONFIG1_ODR_400_HZ               0x0a
#define BMA400_ACC_CONFIG1_ODR_800_HZ               0x0b

#define BMA400_ACC_CONFIG1_ACC_RANGE_2G             0
#define BMA400_ACC_CONFIG1_ACC_RANGE_4G             1
#define BMA400_ACC_CONFIG1_ACC_RANGE_8G             2
#define BMA400_ACC_CONFIG1_ACC_RANGE_16G            3

#define BMA400_ACC_CONFIG2_DATA_SRC_REG             0x0c
#define BMA400_ACC_CONFIG2_ODR_VARIABLE             0x00
#define BMA400_ACC_CONFIG2_ODR_100_HZ               0x04
#define BMA400_ACC_CONFIG2_ODR_100_HZ_1HZ_BW        0x08

#define BMA400_INT_CONFIG0_DRDY_INT_EN              0x80
#define BMA400_INT_CONFIG0_FWM_INT_EN               0x40
#define BMA400_INT_CONFIG0_FFULL_INT_EN             0x20
#define BMA400_INT_CONFIG0_GEN2_INT_EN              0x08
#define BMA400_INT_CONFIG0_GEN1_INT_EN              0x04
#define BMA400_INT_CONFIG0_ORIENTCH_INT_EN          0x02

#define BMA400_INT_CONFIG1_LATCH_INT                0x80
#define BMA400_INT_CONFIG1_ACTCH_INT_EN             0x10
#define BMA400_INT_CONFIG1_D_TAP_INT_EN             0x08
#define BMA400_INT_CONFIG1_S_TAP_INT_EN             0x04
#define BMA400_INT_CONFIG1_STEP_INT_EN              0x01

#define BMA400_INT1_MAP_DRDY_INT1                   0x80
#define BMA400_INT1_MAP_FWM_INT1                    0x40
#define BMA400_INT1_MAP_FFULL_INT1                  0x20
#define BMA400_INT1_MAP_IENG_OVERRUN_INT1           0x10
#define BMA400_INT1_MAP_GEN2_INT1                   0x08
#define BMA400_INT1_MAP_GEN1_INT1                   0x04
#define BMA400_INT1_MAP_ORIENTCH_INT1               0x02
#define BMA400_INT1_MAP_WKUP_INT1                   0x01

#define BMA400_INT2_MAP_DRDY_INT2                   0x80
#define BMA400_INT2_MAP_FWM_INT2                    0x40
#define BMA400_INT2_MAP_FFULL_INT2                  0x20
#define BMA400_INT2_MAP_IENG_OVERRUN_INT2           0x10
#define BMA400_INT2_MAP_GEN2_INT2                   0x08
#define BMA400_INT2_MAP_GEN1_INT2                   0x04
#define BMA400_INT2_MAP_ORIENTCH_INT2               0x02
#define BMA400_INT2_MAP_WKUP_INT2                   0x01

#define BMA400_INT12_MAP_ACTCH_INT2                 0x80
#define BMA400_INT12_MAP_TAP_INT2                   0x40
#define BMA400_INT12_MAP_STEP_INT2                  0x10
#define BMA400_INT12_MAP_ACTCH_INT1                 0x08
#define BMA400_INT12_MAP_TAP_INT1                   0x04
#define BMA400_INT12_MAP_STEP_INT1                  0x01

#define BMA400_INT12_IO_CTRL_INT2_OD                0x40
#define BMA400_INT12_IO_CTRL_INT2_LVL               0x20
#define BMA400_INT12_IO_CTRL_INT1_OD                0x04
#define BMA400_INT12_IO_CTRL_INT1_LVL               0x02

#define BMA400_FIFO_CONFIG0_FIFO_Z_EN               0x80
#define BMA400_FIFO_CONFIG0_FIFO_Y_EN               0x40
#define BMA400_FIFO_CONFIG0_FIFO_X_EN               0x20
#define BMA400_FIFO_CONFIG0_FIFO_8BIT_EN            0x10
#define BMA400_FIFO_CONFIG0_FIFO_DATA_SRC           0x08
#define BMA400_FIFO_CONFIG0_FIFO_TIME_EN            0x04
#define BMA400_FIFO_CONFIG0_FIFO_STOP_ON_FULL       0x02
#define BMA400_FIFO_CONFIG0_AUTO_FLUSH              0x01

#define BMA400_FIFO_PWR_CONFIG_FIFO_READ_DISABLE    0x01

#define BMA400_AUTOLOWPOW_0_AUTO_LP_TIMEOUT_THRES   0xFF
#define BMA400_AUTOLOWPOW_1_AUTO_LP_TIMEOUT_THRES   0xF0
#define BMA400_AUTOLOWPOW_1_AUTO_LP_TIMEOUT         0x0C
#define BMA400_AUTOLOWPOW_1_GEN1_INT                0x02
#define BMA400_AUTOLOWPOW_1_DRDY_LOWPOW_TRIG        0x01

#define BMA400_AUTOWAKEUP_0_WAKEUP_TIMEOUT_THRES    0xFF
#define BMA400_AUTOWAKEUP_1_WAKEUP_TIMEOUT_THRES    0xF0
#define BMA400_AUTOWAKEUP_1_WKUP_TIMEOUT            0x04
#define BMA400_AUTOWAKEUP_1_WKUP_INT                0x02

#define BMA400_WKUP_INT_CONFIG0_WKUP_Z_EN           0x80
#define BMA400_WKUP_INT_CONFIG0_WKUP_Y_EN           0x40
#define BMA400_WKUP_INT_CONFIG0_WKUP_X_EN           0x20
#define BMA400_WKUP_INT_CONFIG0_NUM_OF_SAMPLES      0x1C
#define BMA400_WKUP_INT_CONFIG0_WKUP_REFU           0x03

#define BMA400_ORIENTCH_CONFIG0_ORIENT_Z_EN         0x80
#define BMA400_ORIENTCH_CONFIG0_ORIENT_Y_EN         0x40
#define BMA400_ORIENTCH_CONFIG0_ORIENT_X_EN         0x20
#define BMA400_ORIENTCH_CONFIG0_ORIENT_DATA_SRC     0x10
#define BMA400_ORIENTCH_CONFIG0_ORIENT_REFU         0x0c

#define BMA400_GEN1INT_CONFIG0_GEN1_ACT_Z_EN        0x80
#define BMA400_GEN1INT_CONFIG0_GEN1_ACT_Y_EN        0x40
#define BMA400_GEN1INT_CONFIG0_GEN1_ACT_X_EN        0x20
#define BMA400_GEN1INT_CONFIG0_GEN1_DATA_SRC        0x10
#define BMA400_GEN1INT_CONFIG0_GEN1_ACT_REFU        0x0c
#define BMA400_GEN1INT_CONFIG0_GEN1_ACT_HYST        0x03

#define BMA400_GEN1INT_CONFIG1_GEN1_CRITERION_SEL   0x02
#define BMA400_GEN1INT_CONFIG1_GEN1_COMB_SEL        0x01

#define BMA400_GEN2INT_CONFIG0_GEN2_ACT_Z_EN        0x80
#define BMA400_GEN2INT_CONFIG0_GEN2_ACT_Y_EN        0x40
#define BMA400_GEN2INT_CONFIG0_GEN2_ACT_X_EN        0x20
#define BMA400_GEN2INT_CONFIG0_GEN2_DATA_SRC        0x10
#define BMA400_GEN2INT_CONFIG0_GEN2_ACT_REFU        0x0c
#define BMA400_GEN2INT_CONFIG0_GEN2_ACT_HYST        0x03

#define BMA400_GEN2INT_CONFIG1_GEN2_CRITERION_SEL   0x02
#define BMA400_GEN2INT_CONFIG1_GEN2_COMB_SEL        0x01

#define BMA400_ACTCH_CONFIG1_ACTCH_Z_EN             0x80
#define BMA400_ACTCH_CONFIG1_ACTCH_Y_EN             0x40
#define BMA400_ACTCH_CONFIG1_ACTCH_X_EN             0x20
#define BMA400_ACTCH_CONFIG1_ACTCH_DATA_SRC         0x10
#define BMA400_ACTCH_CONFIG1_ACTCH_NPTS             0x0f

#define BMA400_TAP_CONFIG_SEL_AXIS                  0x18
#define BMA400_TAP_CONFIG_TAP_SENSITIVITY           0x07

#define BMA400_TAP_CONFIG1_QUIET_DT                 0x30
#define BMA400_TAP_CONFIG1_QUIET                    0x0c
#define BMA400_TAP_CONFIG1_TICS_TH                  0x03

/* Magical value that is used to initiate a full reset */
#define BMA400_CMD_SOFT_RESET           0xb6
#define BMA400_CMD_FIFO_FLUSH           0xb0
#define BMA400_CMD_STEP_CNT_CLEAR       0xb1

/* Get the chip ID */
int
bma400_get_chip_id(struct bma400 *bma400, uint8_t *chip_id);

/* All three axis types */
typedef enum {
    AXIS_X   = 0,
    AXIS_Y   = 1,
    AXIS_Z   = 2,
    AXIS_ALL = 3,
} bma400_axis_t;

struct bma400_reg_cache {
    union {
        uint8_t regs[64];
        struct {
            uint8_t acc_config0;
            uint8_t acc_config1;
            uint8_t acc_config2;
            uint8_t reserved1[3];
            uint8_t int_config0;
            uint8_t int_config1;
            uint8_t int1_map;
            uint8_t int2_map;
            uint8_t int12_map;
            uint8_t int12_io_ctrl;
            uint8_t reserved2[1];
            uint8_t fifo_config0;
            uint8_t fifo_config1;
            uint8_t fifo_config2;
            uint8_t fifo_pwr_config;
            uint8_t autolowpow_0;
            uint8_t autolowpow_1;
            uint8_t autowakeup_0;
            uint8_t autowakeup_1;
            uint8_t reserved3[1];
            uint8_t wkup_int_config0;
            uint8_t wkup_int_config1;
            uint8_t wkup_int_config2;
            uint8_t wkup_int_config3;
            uint8_t wkup_int_config4;
            uint8_t reserved4[1];
            uint8_t orientch_config0;
            uint8_t orientch_config1;
            uint8_t reserved5[1];
            uint8_t orientch_config3;
            uint8_t orientch_config4;
            uint8_t orientch_config5;
            uint8_t orientch_config6;
            uint8_t orientch_config7;
            uint8_t orientch_config8;
            uint8_t orientch_config9;
            uint8_t gen1int_config0;
            uint8_t gen1int_config1;
            uint8_t gen1int_config2;
            uint8_t gen1int_config3;
            uint8_t gen1int_config31;
            uint8_t gen1int_config4;
            uint8_t gen1int_config5;
            uint8_t gen1int_config6;
            uint8_t gen1int_config7;
            uint8_t gen1int_config8;
            uint8_t gen1int_config9;
            uint8_t gen2int_config0;
            uint8_t gen2int_config1;
            uint8_t gen2int_config2;
            uint8_t gen2int_config3;
            uint8_t gen2int_config31;
            uint8_t gen2int_config4;
            uint8_t gen2int_config5;
            uint8_t gen2int_config6;
            uint8_t gen2int_config7;
            uint8_t gen2int_config8;
            uint8_t gen2int_config9;
            uint8_t acth_config0;
            uint8_t acth_config1;
            uint8_t tap_config0;
            uint8_t tap_config1;
        };
    };
    uint64_t dirty;
    bool always_read;
};

/* Active status of all interrupts */
struct bma400_int_status {
    uint8_t int_stat0;
    uint8_t int_stat1;
    uint8_t int_stat2;
};

/* Kick off a full soft reset of the device */
int
bma400_soft_reset(struct bma400 *bma400);

/* Drive mode of the interrupt pins */
enum int_pin_output {
    INT_PIN_OUTPUT_PUSH_PULL  = 0,
    INT_PIN_OUTPUT_OPEN_DRAIN = 1,
};

/* Active mode of the interrupt pins */
enum int_pin_active {
    INT_PIN_ACTIVE_LOW  = 0,
    INT_PIN_ACTIVE_HIGH = 1,
};

/* Electrical settings of both interrupt pins */
struct int_pin_electrical {
    enum int_pin_output pin1_output;
    enum int_pin_active pin1_active;
    enum int_pin_output pin2_output;
    enum int_pin_active pin2_active;
};

/* Get/Set the FIFO watermark level */
int
bma400_get_fifo_watermark(struct bma400 *bma400,
                          uint16_t *watermark);
int
bma400_set_fifo_watermark(struct bma400 *bma400,
                          uint16_t watermark);

int bma400_get_register(struct bma400 *bma400, uint8_t reg, uint8_t *data);
int bma400_set_register(struct bma400 *bma400, uint8_t reg, uint8_t data);
int bma400_set_register_field(struct bma400 *bma400, uint8_t reg,
                              uint8_t field_mask, uint8_t field_val);
int bma400_read(struct bma400 *bma400, uint8_t reg, uint8_t *buffer, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif
