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

#ifndef __LED_ITF_H__
#define __LED_ITF_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * LED interfaces
 */
#define LED_ITF_SPI    (0)
#define LED_ITF_I2C    (1)
#define LED_ITF_UART   (2)

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
