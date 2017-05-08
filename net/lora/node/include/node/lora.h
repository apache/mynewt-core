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

#ifndef H_LORA_
#define H_LORA_

#include "stats/stats.h"

STATS_SECT_START(lora_stats)
    STATS_SECT_ENTRY(rx_error)
    STATS_SECT_ENTRY(rx_success)
    STATS_SECT_ENTRY(rx_timeout)
    STATS_SECT_ENTRY(tx_success)
    STATS_SECT_ENTRY(tx_timeout)
STATS_SECT_END
extern STATS_SECT_DECL(lora_stats) lora_stats;

#endif
