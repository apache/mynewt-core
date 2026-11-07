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

/* clang-format off */

/*
 * TCM sizes come from MCU_DTCM_SIZE / MCU_ITCM_SIZE.
 * When STACK_REGION is defined (mcu_config.ld.h / bsp_config.ld.h)
 * stack is placed at the end of DTCM.
 */
#ifdef STACK_REGION
    DTCM (rw!x) : ORIGIN = 0x20000000,
                 LENGTH = MYNEWT_VAL(MCU_DTCM_SIZE) - STACK_SIZE
    STACK_RAM (rw) : ORIGIN = 0x20000000 + MYNEWT_VAL(MCU_DTCM_SIZE) - STACK_SIZE,
                     LENGTH = STACK_SIZE
#else
    DTCM (rw!x) : ORIGIN = 0x20000000, LENGTH = MYNEWT_VAL(MCU_DTCM_SIZE)
#endif
    ITCM (rwx)  : ORIGIN = 0x00000000, LENGTH = MYNEWT_VAL(MCU_ITCM_SIZE)

/* External SDRAM, when hw/drivers/ram/stm32_sdram is in the build */
#if MYNEWT_VAL(STM32_SDRAM_SIZE)
#if MYNEWT_VAL(STM32_SDRAM_BANK) == 2
    SDRAM (rw) : ORIGIN = 0xD0000000, LENGTH = MYNEWT_VAL(STM32_SDRAM_SIZE)
#else
    SDRAM (rw) : ORIGIN = 0xC0000000, LENGTH = MYNEWT_VAL(STM32_SDRAM_SIZE)
#endif
#endif
