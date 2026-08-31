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

#include <syscfg/syscfg.h>

#if MYNEWT_VAL_APOLLO3_SRAM_RESV > 0
#define RAM_SIZE                                                              \
    (MYNEWT_VAL_MCU_RAM_SIZE - MYNEWT_VAL_APOLLO3_SRAM_RESV -                 \
     MYNEWT_VAL_APOLLO3_SBL_STACK_MARGIN)
#define NOINIT_RAM NOINIT_REGION
#else
#define RAM_SIZE                                                              \
    (MYNEWT_VAL_MCU_RAM_SIZE - MYNEWT_VAL_APOLLO3_SBL_STACK_MARGIN)
#define NOINIT_RAM RAM
#endif
