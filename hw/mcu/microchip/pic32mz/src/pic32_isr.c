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

#include <xc.h>
#include <mcu/pic32_isr.h>

/*
 * Each IPCx register holds priority of 4 vectors, 8 bits per vector:
 * bits 1:0 subpriority, bits 4:2 priority.
 * IPCx registers are 0x10 bytes apart (IPCx, IPCxCLR, IPCxSET, IPCxINV).
 */
#define IPC_REG_STRIDE  4
#define IPC_CLR         1
#define IPC_SET         2
#define IPC_FIELD_MASK  0x1F
#define IPC_IP_POS      2

void
pic32_isr_init(void)
{
    const struct pic32_isr_cfg *cfg;
    volatile uint32_t *ipc;
    uint32_t shift;

    for (cfg = LINK_TABLE_START(pic32_isr_table);
         cfg < LINK_TABLE_END(pic32_isr_table); ++cfg) {
        ipc = &IPC0 + (cfg->vector / 4) * IPC_REG_STRIDE;
        shift = (cfg->vector % 4) * 8;
        ipc[IPC_CLR] = IPC_FIELD_MASK << shift;
        ipc[IPC_SET] = (cfg->ipl << IPC_IP_POS) << shift;
    }
}
