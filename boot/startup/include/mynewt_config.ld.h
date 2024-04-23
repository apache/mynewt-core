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

#ifndef MYNEWT_CONFIG_LD_H
#define MYNEWT_CONFIG_LD_H

/* Default destination for RTT section */
#ifndef RTT_RAM
#define RTT_RAM RAM
#endif

/* Default destination section for function that should be execute from RAM */
#ifndef TEXT_RAM
#define TEXT_RAM RAM
#endif

#ifndef VECTOR_RELOCATION_RAM
#define VECTOR_RELOCATION_RAM RAM
#endif
/*#define COREDATA_RAM RAM*/
/*#define COREBSS_RAM RAM*/
#ifndef DATA_RAM
#define DATA_RAM RAM
#endif
#ifndef BSS_RAM
#define BSS_RAM RAM
#endif
#ifndef BSSNZ_RAM
#define BSSNZ_RAM RAM
#endif

#ifndef MYNEWT_VAL_RESET_HANDLER
#define RESET_HANDLER Reset_Handler
#else
#define RESET_HANDLER MYNEWT_VAL_RESET_HANDLER
#endif

#ifndef MYNEWT_VAL_MAIN_STACK_SIZE
#define STACK_SIZE 0x800
#else
#define STACK_SIZE MYNEWT_VAL_MAIN_STACK_SIZE
#endif

#ifndef RAM_START
#define RAM_START MYNEWT_VAL_MCU_RAM_START
#endif
#ifndef RAM_SIZE
#define RAM_SIZE MYNEWT_VAL_MCU_RAM_SIZE
#endif

#ifndef MYNEWT_CODE
#if MYNEWT_VAL_RAM_RESIDENT
#define MYNEWT_CODE RAM
#elif MYNEWT_VAL_BOOT_LOADER
#define MYNEWT_CODE BOOT
#else
#define MYNEWT_CODE SLOT0
#endif
#endif

#endif
