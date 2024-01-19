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

#ifndef weak_alias
#define weak_alias(name, aliasname) \
   extern __typeof (name) aliasname __attribute__ ((weak, alias (#name)));
#endif

extern void Reset_Handler(void);

void
Default_Handler(void) {
    while(1);
}

void
Default_NMI_Handler(void) {
    while(1);
}

void
Default_HardFault_Handler(void) {
    while(1);
}

void
Default_MemManage_Handler(void) {
    while(1);
}

void
Default_BusFault_Handler(void) {
    while(1);
}

void
Default_UsageFault_Handler(void) {
    while(1);
}

void
Default_SecureFault_Handler(void) {
    while(1);
}

void
Default_SVC_Handler(void) {
    while(1);
}

void
Default_DebugMon_Handler(void) {
    while(1);
}

void
Default_PendSV_Handler(void) {
    while(1);
}

void
Default_SysTick_Handler(void) {
    while(1);
}

#define INT_VECTOR_STACK_TOP(stack_top) extern void stack_top(void);
#define INT_VECTOR_RESET_HANDLER(reset_handler)
#define INT_VECTOR_NMI_HANDLER(handler) weak_alias(Default_NMI_Handler, handler)
#define INT_VECTOR_HARDFAULT_HANDLER(handler) weak_alias(Default_HardFault_Handler, handler)
#define INT_VECTOR_MEMMANAGE_HANDLER(handler) weak_alias(Default_MemManage_Handler, handler)
#define INT_VECTOR_BUSFAULT_HANDLER(handler) weak_alias(Default_BusFault_Handler, handler)
#define INT_VECTOR_USAGEFAULT_HANDLER(handler) weak_alias(Default_UsageFault_Handler, handler)
#define INT_VECTOR_SECUREFAULT_HANDLER(handler) weak_alias(Default_SecureFault_Handler, handler)
#define INT_VECTOR_SVC_HANDLER(handler) weak_alias(Default_SVC_Handler, handler)
#define INT_VECTOR_DEBUGMON_HANDLER(handler) weak_alias(Default_DebugMon_Handler, handler)
#define INT_VECTOR_PENDSV_HANDLER(handler) weak_alias(Default_PendSV_Handler, handler)
#define INT_VECTOR_SYSTICK_HANDLER(handler) weak_alias(Default_SysTick_Handler, handler)
#define INT_VECTOR(isr) weak_alias(Default_Handler, isr)
#define INT_VECTOR_UNUSED(a)

#include "mcu/mcu_vectors.h"

#undef INT_VECTOR_STACK_TOP
#undef INT_VECTOR_RESET_HANDLER
#undef INT_VECTOR_NMI_HANDLER
#undef INT_VECTOR_HARDFAULT_HANDLER
#undef INT_VECTOR_MEMMANAGE_HANDLER
#undef INT_VECTOR_BUSFAULT_HANDLER
#undef INT_VECTOR_USAGEFAULT_HANDLER
#undef INT_VECTOR_SECUREFAULT_HANDLER
#undef INT_VECTOR_SVC_HANDLER
#undef INT_VECTOR_DEBUGMON_HANDLER
#undef INT_VECTOR_PENDSV_HANDLER
#undef INT_VECTOR_SYSTICK_HANDLER
#undef INT_VECTOR
#undef INT_VECTOR_UNUSED

#define INT_VECTOR_STACK_TOP(stack_top) stack_top,
#define INT_VECTOR_RESET_HANDLER(handler) handler,
#define INT_VECTOR_NMI_HANDLER(handler) handler,
#define INT_VECTOR_HARDFAULT_HANDLER(handler) handler,
#define INT_VECTOR_MEMMANAGE_HANDLER(handler) handler,
#define INT_VECTOR_BUSFAULT_HANDLER(handler) handler,
#define INT_VECTOR_USAGEFAULT_HANDLER(handler) handler,
#define INT_VECTOR_SECUREFAULT_HANDLER(handler) handler,
#define INT_VECTOR_SVC_HANDLER(handler) handler,
#define INT_VECTOR_DEBUGMON_HANDLER(handler) handler,
#define INT_VECTOR_PENDSV_HANDLER(handler) handler,
#define INT_VECTOR_SYSTICK_HANDLER(handler) handler,
#define INT_VECTOR(isr) isr,
#define INT_VECTOR_UNUSED(a) 0,
void (*g_pfnVectors[])(void) __attribute__((section(".isr_vector"))) = {
#include "mcu/mcu_vectors.h"
};

