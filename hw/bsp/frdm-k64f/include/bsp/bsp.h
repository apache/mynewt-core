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
#ifndef H_BSP_H
#define H_BSP_H

#include <stdint.h>
#include <mcu/mcu.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t __DATA_ROM;
#define RAM_SIZE            0x40000

/* LED pins */
#define LED_BLINK_PIN       MCU_GPIO_PORTB(22)

/* This defines the maximum NFFS areas (block) are in the BSPs NFS file
 * system space.  This in conjunction with flash map determines how
 * many NFS blocks there will be.  A minimum is the number of individually
 * erasable sectors in the flash area and the maximum is this number. If
 * your max is less than the number of sectors then the NFFS will combine
 * multiple sectors into an NFFS area */
#define NFFS_AREA_MAX    (8)

#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
