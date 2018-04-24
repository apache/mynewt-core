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

#include "os/mynewt.h"

/* These definitions are required to include the code for a given region */
#undef REGION_AS923
#undef REGION_AU915
#undef REGION_CN470
#undef REGION_CN779
#undef REGION_EU433
#undef REGION_EU868
#undef REGION_KR920
#undef REGION_IN865
#undef REGION_US915
#undef REGION_US915_HYBRID

/* This defines the currently active region */
#if   MYNEWT_VAL(LORA_NODE_REGION) == 1
#define LORA_NODE_REGION    LORAMAC_REGION_AS923
#define REGION_AS923
#elif MYNEWT_VAL(LORA_NODE_REGION) == 2
#define LORA_NODE_REGION    LORAMAC_REGION_AU915
#define REGION_AU915
#elif MYNEWT_VAL(LORA_NODE_REGION) == 3
#define LORA_NODE_REGION LORAMAC_REGION_CN470
#define REGION_CN470
#elif MYNEWT_VAL(LORA_NODE_REGION) == 4
#define LORA_NODE_REGION LORAMAC_REGION_CN779
#define REGION_CN779
#elif MYNEWT_VAL(LORA_NODE_REGION) == 5
#define LORA_NODE_REGION LORAMAC_REGION_EU433
#define REGION_EU433
#elif MYNEWT_VAL(LORA_NODE_REGION) == 6
#define LORA_NODE_REGION LORAMAC_REGION_EU868
#define REGION_EU868
#elif MYNEWT_VAL(LORA_NODE_REGION) == 7
#define LORA_NODE_REGION LORAMAC_REGION_KR920
#define REGION_KR920
#elif MYNEWT_VAL(LORA_NODE_REGION) == 8
#define LORA_NODE_REGION LORAMAC_REGION_IN865
#define REGION_IN865
#elif MYNEWT_VAL(LORA_NODE_REGION) == 9
#define LORA_NODE_REGION LORAMAC_REGION_US915
#define REGION_US915
#elif MYNEWT_VAL(LORA_NODE_REGION) == 10
#define LORA_NODE_REGION LORAMAC_REGION_US915_HYBRID
#define REGION_US915_HYBRID
#else
#error "Unknown region specified!"
#endif

#endif
