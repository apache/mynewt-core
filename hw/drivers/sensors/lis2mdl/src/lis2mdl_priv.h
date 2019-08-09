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

#ifndef __LIS2MDL_PRIV_H__
#define __LIS2MDL_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

/* I2C Slave Addess for LIS2MDL Mag */
#define LIS2MDL_ADDR              0x1e

#define LIS2MDL_WHO_AM_I_VAL      0x40
#define LIS2MDL_SENSITIVITY       1.5f
#define LIS2MDL_LSB_TO_UTESLA(_g) (_g * LIS2MDL_SENSITIVITY * 100.0f)

enum lis2mdl_registers_mag {
    LIS2MDL_OFFSET_X_REG_L      = 0x45,
    LIS2MDL_OFFSET_X_REG_H      = 0x46,
    LIS2MDL_OFFSET_Y_REG_L      = 0x47,
    LIS2MDL_OFFSET_Y_REG_H      = 0x48,
    LIS2MDL_OFFSET_Z_REG_L      = 0x49,
    LIS2MDL_OFFSET_Z_REG_H      = 0x4a,
    LIS2MDL_WHO_AM_I            = 0x4f,
    LIS2MDL_CFG_REG_A           = 0x60,
    LIS2MDL_CFG_REG_B           = 0x61,
    LIS2MDL_CFG_REG_C           = 0x62,
    LIS2MDL_INT_CTRL_REG        = 0x63,
    LIS2MDL_INT_SOURCE_REG      = 0x64,
    LIS2MDL_INT_THS_L_REG       = 0x65,
    LIS2MDL_INT_THS_H_REG       = 0x66,
    LIS2MDL_STATUS_REG          = 0x67,
    LIS2MDL_OUTX_L_REG          = 0x68
};

/* Bitmasks for LIS2MDL_CFG_REG_A */
#define LIS2MDL_MODE_MASK         0x03
#define LIS2MDL_ODR_MASK          0x0c
#define LIS2MDL_SOFT_RST_MASK     0x20
#define LIS2MDL_REBOOT_MASK       0x40
#define LIS2MDL_COMP_TEMP_EN_MASK 0x80

/* Bitmasks for LIS2MDL_CFG_REG_C */
#define LIS2MDL_INT_MAG_MASK      0x01
#define LIS2MDL_BDU_MASK          0x10

/* Bitmasks for LIS2MDL_STATUS_REG */
#define LIS2MDL_ZYXDA_MASK        0x08

#ifdef __cplusplus
}
#endif

#endif /* __LIS2MDL_PRIV_H__ */
