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

#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "node/lora.h"
#include "node/lora_priv.h"

STATS_SECT_DECL(lora_stats) lora_stats;
STATS_NAME_START(lora_stats)
    STATS_NAME(lora_stats, rx_error)
    STATS_NAME(lora_stats, rx_success)
    STATS_NAME(lora_stats, rx_timeout)
    STATS_NAME(lora_stats, tx_success)
    STATS_NAME(lora_stats, tx_timeout)
STATS_NAME_END(lora_stats)

void
lora_node_init(void)
{
    int rc;

    rc = stats_init_and_reg(
        STATS_HDR(lora_stats),
        STATS_SIZE_INIT_PARMS(lora_stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lora_stats), "lora");
    SYSINIT_PANIC_ASSERT(rc == 0);

#if MYNEWT_VAL(LORA_NODE_CLI)
    lora_cli_init();
#endif
}
