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

#ifndef __STM32_ETH_H__
#define __STM32_ETH_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize the ethernet device with cfg.
 *
 * @param cfg HW specific configuration.
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int stm32_eth_init(const void *cfg);

/*
 * Set the MAC address for ethernet to use.
 *
 * @param addr Byte array of 6 bytes to use as MAC address.
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int stm32_eth_set_hwaddr(uint8_t *addr);

#ifdef __cplusplus
}
#endif

#endif /* __STM32_ETH_H__ */
