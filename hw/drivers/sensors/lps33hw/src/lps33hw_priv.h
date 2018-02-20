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

#ifndef __LPS33HW_PRIV_H__
#define __LPS33HW_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

enum lps33hw_registers {
    LPS33HW_INTERRUPT_CFG = 0x0b,
    LPS33HW_THS_P_L = 0x0c,
    LPS33HW_THS_P_H = 0x0d,
    LPS33HW_WHO_AM_I = 0x0f,
    LPS33HW_CTRL_REG1 = 0x10,
    LPS33HW_CTRL_REG2 = 0x11,
    LPS33HW_CTRL_REG3 = 0x12,
    LPS33HW_FIFO_CTRL = 0x14,
    LPS33HW_REF_P_XL = 0x15,
    LPS33HW_REF_P_L = 0x16,
    LPS33HW_REF_P_H = 0x17,
    LPS33HW_RPDS_L = 0x18,
    LPS33HW_RPDS_H = 0x19,
    LPS33HW_RES_CONF = 0x1a,
    LPS33HW_INT_SOURCE = 0x25,
    LPS33HW_FIFO_STATUS = 0x26,
    LPS33HW_STATUS = 0x27,
    LPS33HW_PRESS_OUT_XL = 0x28,
    LPS33HW_PRESS_OUT_L = 0x29,
    LPS33HW_PRESS_OUT_H = 0x2a,
    LPS33HW_TEMP_OUT_L = 0x2b,
    LPS33HW_TEMP_OUT_H = 0x2c,
    LPS33HW_LPFP_RES = 0x33
};

#define LPS33HW_WHO_AM_I_VAL (0xb1)

int lps33hw_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value);
int lps33hw_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value);
int lps33hw_read24(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer);

#ifdef __cplusplus
}
#endif

#endif /* __LPS33HW_PRIV_H__ */
