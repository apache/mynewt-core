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

#ifndef H_LORA_BAND_
#define H_LORA_BAND_

#include "syscfg/syscfg.h"

#if   MYNEWT_VAL(LORA_NODE_FREQ_BAND) == 433
#define USE_BAND_433

#elif MYNEWT_VAL(LORA_NODE_FREQ_BAND) == 470
#define USE_BAND_470

#elif MYNEWT_VAL(LORA_NODE_FREQ_BAND) == 780
#define USE_BAND_780

#elif MYNEWT_VAL(LORA_NODE_FREQ_BAND) == 868
#define USE_BAND_868

#elif MYNEWT_VAL(LORA_NODE_FREQ_BAND) == 915
#define USE_BAND_915

#endif

#endif
