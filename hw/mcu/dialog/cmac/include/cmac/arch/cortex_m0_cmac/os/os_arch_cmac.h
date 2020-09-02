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

#ifndef _OS_ARCH_CMAC_H
#define _OS_ARCH_CMAC_H

#ifdef __cplusplus
extern "C" {
#endif

/* See os_arch_cmac.c */

#undef NVIC_EnableIRQ
#undef NVIC_GetEnableIRQ
#undef NVIC_DisableIRQ

#define NVIC_EnableIRQ          os_arch_cmac_enable_irq
#define NVIC_GetEnableIRQ       os_arch_cmac_get_enable_irq
#define NVIC_DisableIRQ         os_arch_cmac_disable_irq

void os_arch_cmac_enable_irq(IRQn_Type irqn);
uint32_t os_arch_cmac_get_enable_irq(IRQn_Type irqn);
void os_arch_cmac_disable_irq(IRQn_Type irqn);
uint32_t os_arch_cmac_pending_irq(void);

void os_arch_cmac_wfi(void);
int os_arch_cmac_deep_sleep(void);
void os_arch_cmac_pendsvset(void);

void os_arch_cmac_bs_ctrl_irq_block(void);
void os_arch_cmac_bs_ctrl_irq_unblock(void);

void os_arch_cmac_idle_section_enter(void);
void os_arch_cmac_idle_section_exit(void);

#ifdef __cplusplus
}
#endif

#endif /* _OS_ARCH_CMAC_H */
