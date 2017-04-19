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

#ifndef __STM32_ETH_CFG_H__
#define __STM32_ETH_CFG_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * BSP specific ethernet settings.
 */
#define STM32_MAX_PORTS	9

struct stm32_eth_cfg {
    /* Mask of pins from ports A-I to use */
    uint32_t sec_port_mask[STM32_MAX_PORTS];
    enum {
        SMSC_8710_RMII,
        LAN_8742_RMII
    } sec_phy_type;
    int sec_phy_irq;
    uint8_t sec_phy_addr;
};

#ifdef __cplusplus
}
#endif

#endif /* __STM32_ETH_CFG_H__ */
