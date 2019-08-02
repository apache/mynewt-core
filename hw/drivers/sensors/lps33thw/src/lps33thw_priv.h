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

#ifndef REPOS_APACHE_MYNEWT_CORE_HW_DRIVERS_SENSORS_LPS33THW_SRC_LPS33THW_PRIV_H_
#define REPOS_APACHE_MYNEWT_CORE_HW_DRIVERS_SENSORS_LPS33THW_SRC_LPS33THW_PRIV_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Max time to wait for interrupt */
#define LPS33THW_MAX_INT_WAIT (4 * OS_TICKS_PER_SEC)

enum lps33thw_registers {
    LPS33THW_INTERRUPT_CFG = 0x0b,
    LPS33THW_THS_P = 0x0c,
    LPS33THW_IF_CTRL = 0x0e,
    LPS33THW_WHO_AM_I = 0x0f,
    LPS33THW_CTRL_REG1 = 0x10,
    LPS33THW_CTRL_REG2 = 0x11,
    LPS33THW_CTRL_REG3 = 0x12,
    LPS33THW_FIFO_CTRL = 0x13,
    LPS33THW_FIFO_WTM = 0x14,
    LPS33THW_REF_P = 0x15,
    LPS33THW_RPDS = 0x18,
    LPS33THW_INT_SOURCE = 0x24,
    LPS33THW_FIFO_STATUS1 = 0x25,
    LPS33THW_FIFO_STATUS2 = 0x26,
    LPS33THW_STATUS = 0x27,
    LPS33THW_PRESS_OUT = 0x28,
    LPS33THW_TEMP_OUT = 0x2b,
    LPS33THW_FIFO_SDATA_OUT_PRESS = 0x78,
    LPS33THW_FIFO_SDATA_OUT_TEMP = 0x7b,
};

enum lps33thw_fifo_mode {
    LPS33THW_FIFO_BYPASS = 0x00,
    LPS33THW_FIFO_CONTINUOUS = 0x02,
};

/* registers */
struct lps33thw_register_value {
    uint8_t reg;
    uint8_t pos;
    uint8_t mask;
};

#define LPS33THW_INT_LOW_BIT (0x01)
#define LPS33THW_INT_HIGH_BIT (0x01)

#define LPS33THW_REGISTER_VALUE(r, n, p, m) \
static const struct lps33thw_register_value n = {.reg = r, .pos = p, .mask = m}

LPS33THW_REGISTER_VALUE(LPS33THW_INTERRUPT_CFG, LPS33THW_INTERRUPT_CFG_AUTORIFP, 7, 0x80);
LPS33THW_REGISTER_VALUE(LPS33THW_INTERRUPT_CFG, LPS33THW_INTERRUPT_CFG_RESET_ARP, 6, 0x40);
LPS33THW_REGISTER_VALUE(LPS33THW_INTERRUPT_CFG, LPS33THW_INTERRUPT_CFG_AUTOZERO, 5, 0x20);
LPS33THW_REGISTER_VALUE(LPS33THW_INTERRUPT_CFG, LPS33THW_INTERRUPT_CFG_RESET_AZ, 4, 0x10);
LPS33THW_REGISTER_VALUE(LPS33THW_INTERRUPT_CFG, LPS33THW_INTERRUPT_CFG_DIFF_EN, 3, 0x08);
LPS33THW_REGISTER_VALUE(LPS33THW_INTERRUPT_CFG, LPS33THW_INTERRUPT_CFG_LIR, 2, 0x04);
LPS33THW_REGISTER_VALUE(LPS33THW_INTERRUPT_CFG, LPS33THW_INTERRUPT_CFG_PLE, 1, 0x02);
LPS33THW_REGISTER_VALUE(LPS33THW_INTERRUPT_CFG, LPS33THW_INTERRUPT_CFG_PHE, 0, 0x01);

LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG1, LPS33THW_CTRL_REG1_ODR, 4, 0x70);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG1, LPS33THW_CTRL_REG1_EN_LPFP, 3, 0x04);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG1, LPS33THW_CTRL_REG1_LPFP_CFG, 2, 0x03);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG1, LPS33THW_CTRL_REG1_BDU, 1, 0x02);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG1, LPS33THW_CTRL_REG1_SIM, 0, 0x01);

LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG2, LPS33THW_CTRL_REG2_BOOT, 7, 0x80);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG2, LPS33THW_CTRL_REG2_INT_H_L, 6, 0x40);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG2, LPS33THW_CTRL_REG2_PP_OD, 5, 0x20);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG2, LPS33THW_CTRL_REG2_IF_ADD_INC, 4, 0x10);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG2, LPS33THW_CTRL_REG2_SWRESET, 2, 0x04);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG2, LPS33THW_CTRL_REG2_LOW_NOISE_EN, 1, 0x02);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG2, LPS33THW_CTRL_REG2_ONE_SHOT, 0, 0x01);

LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG3, LPS33THW_CTRL_REG3_F_FULL, 5, 0x20);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG3, LPS33THW_CTRL_REG3_F_FTH, 4, 0x10);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG3, LPS33THW_CTRL_REG3_F_OVR, 3, 0x08);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG3, LPS33THW_CTRL_REG3_DRDY, 2, 0x04);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG3, LPS33THW_CTRL_REG3_FIFO_WTM, 4, 0x10);
LPS33THW_REGISTER_VALUE(LPS33THW_CTRL_REG3, LPS33THW_CTRL_REG3_INT_S, 0, 0x03);

LPS33THW_REGISTER_VALUE(LPS33THW_FIFO_CTRL, LPS33THW_FIFO_CTRL_MODE, 0, 0x07);

LPS33THW_REGISTER_VALUE(LPS33THW_FIFO_WTM, LPS33THW_FIFO_WTM_THR, 0, 0x7f);

LPS33THW_REGISTER_VALUE(LPS33THW_INT_SOURCE, LPS33THW_INT_SOURCE_IA, 2, 0x04);
LPS33THW_REGISTER_VALUE(LPS33THW_INT_SOURCE, LPS33THW_INT_SOURCE_PL, 1, 0x02);
LPS33THW_REGISTER_VALUE(LPS33THW_INT_SOURCE, LPS33THW_INT_SOURCE_PH, 0, 0x01);

LPS33THW_REGISTER_VALUE(LPS33THW_STATUS, LPS33THW_STATUS_T_OR, 5, 0x20);
LPS33THW_REGISTER_VALUE(LPS33THW_STATUS, LPS33THW_STATUS_P_OR, 5, 0x10);
LPS33THW_REGISTER_VALUE(LPS33THW_STATUS, LPS33THW_STATUS_T_DA, 5, 0x02);
LPS33THW_REGISTER_VALUE(LPS33THW_STATUS, LPS33THW_STATUS_P_DA, 5, 0x01);

#define LPS33THW_WHO_AM_I_VAL (0xb3)

#ifdef __cplusplus
}
#endif



#endif /* REPOS_APACHE_MYNEWT_CORE_HW_DRIVERS_SENSORS_LPS33THW_SRC_LPS33THW_PRIV_H_ */
