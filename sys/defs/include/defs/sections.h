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

#ifndef H_DEFS_SECTIONS_
#define H_DEFS_SECTIONS_

#include <mcu/mcu.h>
#include <bsp/bsp.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * BSP/MCU can specify special sections it wants to use.
 * But it does not have to, and if it doesn't define them, then here is the
 * default.
 */
/*
 * XXX explanation of when to use these 3?
 */
#ifndef sec_data_core
#define sec_data_core
#endif
#ifndef sec_bss_core
#define sec_bss_core
#endif
#ifndef sec_bss_nz_core
#define sec_bss_nz_core
#endif

/* Code which should be placed and executed from RAM */
#ifndef sec_text_ram_core
#define sec_text_ram_core
#endif

/**
 * Sensitive data (eg keys), which should be excluded from corefiles.
 */
#ifndef sec_bss_secret
#define sec_bss_secret
#endif
#ifndef sec_data_secret
#define sec_data_secret
#endif

#ifdef __cplusplus
}
#endif


#endif /* H_DEFS_SECTIONS_ */
