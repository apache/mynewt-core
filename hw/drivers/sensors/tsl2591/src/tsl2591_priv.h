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
 
#ifndef __TSL2591_PRIV_H__
#define __TSL2591_PRIV_H__

#define TSL2591_REGISTER_ENABLE             (0x00) /*< Enable register */
#define TSL2591_REGISTER_CONTROL            (0x01) /*< Control register */
#define TSL2591_REGISTER_THRESHOLD_AILTL    (0x04) /*< ALS low threshold lower byte */
#define TSL2591_REGISTER_THRESHOLD_AILTH    (0x05) /*< ALS low threshold upper byte */
#define TSL2591_REGISTER_THRESHOLD_AIHTL    (0x06) /*< ALS high threshold lower byte */
#define TSL2591_REGISTER_THRESHOLD_AIHTH    (0x07) /*< ALS high threshold upper byte */
#define TSL2591_REGISTER_THRESHOLD_NPAILTL  (0x08) /*< No Persist ALS low threshold lower byte */
#define TSL2591_REGISTER_THRESHOLD_NPAILTH  (0x09) /*< No Persist ALS low threshold higher byte */
#define TSL2591_REGISTER_THRESHOLD_NPAIHTL  (0x0A) /*< No Persist ALS high threshold lower byte */
#define TSL2591_REGISTER_THRESHOLD_NPAIHTH  (0x0B) /*< No Persist ALS high threshold higher byte */
#define TSL2591_REGISTER_PERSIST_FILTER     (0x0C) /*< Interrupt persistence filter */
#define TSL2591_REGISTER_PACKAGE_PID        (0x11) /*< Package Identification */
#define TSL2591_REGISTER_DEVICE_ID          (0x12) /*< Device Identification */
#define TSL2591_REGISTER_DEVICE_STATUS      (0x13) /*< Internal Status */
#define TSL2591_REGISTER_CHAN0_LOW          (0x14) /*< Channel 0 data, low byte */
#define TSL2591_REGISTER_CHAN0_HIGH         (0x15) /*< Channel 0 data, high byte */
#define TSL2591_REGISTER_CHAN1_LOW          (0x16) /*< Channel 1 data, low byte */
#define TSL2591_REGISTER_CHAN1_HIGH         (0x17) /*< Channel 1 data, high byte */

#define TSL2591_VISIBLE           (2)      /*< (channel 0) - (channel 1) */
#define TSL2591_INFRARED          (1)      /*< channel 1 */
#define TSL2591_FULLSPECTRUM      (0)      /*< channel 0 */

#define TSL2591_COMMAND_BIT       (0xA0)   /*< 1010 0000: bits 7 and 5 for 'command normal' */

#define TSL2591_CLEAR_INT         (0xE7)   /*! Special Function Command for "Clear ALS and no persist ALS interrupt" */
#define TSL2591_TEST_INT          (0xE4)   /*! Special Function Command for "Interrupt set - forces an interrupt" */

#define TSL2591_WORD_BIT          (0x20)   /*< 1 = read/write word (rather than byte) */
#define TSL2591_BLOCK_BIT         (0x10)   /*< 1 = using block read/write */

#define TSL2591_ENABLE_POWEROFF   (0x00)   /*< Flag for ENABLE register to disable */
#define TSL2591_ENABLE_POWERON    (0x01)   /*< Flag for ENABLE register to enable */
#define TSL2591_ENABLE_AEN        (0x02)   /*< ALS Enable. This field activates ALS function. Writing a one activates the ALS. Writing a zero disables the ALS. */
#define TSL2591_ENABLE_AIEN       (0x10)   /*< ALS Interrupt Enable. When asserted permits ALS interrupts to be generated, subject to the persist filter. */
#define TSL2591_ENABLE_NPIEN      (0x80)   /*< No Persist Interrupt Enable. When asserted NP Threshold conditions will generate an interrupt, bypassing the persist filter */

#define TSL2591_LUX_DF            (408.0F) /*< Lux cooefficient */
#define TSL2591_LUX_COEFB         (1.64F)  /*< CH0 coefficient */
#define TSL2591_LUX_COEFC         (0.59F)  /*< CH1 coefficient A */
#define TSL2591_LUX_COEFD         (0.86F)  /*< CH2 coefficient B */

#ifdef __cplusplus
extern "C" {
#endif

int tsl2591_write8(struct sensor_itf *itf, uint8_t reg, uint32_t value);
int tsl2591_write16(struct sensor_itf *itf, uint8_t reg, uint16_t value);
int tsl2591_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value);
int tsl2591_read16(struct sensor_itf *itf, uint8_t reg, uint16_t *value);

#ifdef __cplusplus
}
#endif

#endif /* __TSL2591_PRIV_H_ */
