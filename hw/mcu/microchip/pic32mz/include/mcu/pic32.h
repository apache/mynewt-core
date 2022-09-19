/**
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

#ifndef __MCU_PIC32_H__
#define __MCU_PIC32_H__

extern uint32_t SystemCoreClock;

#if __PIC32_HAS_L1CACHE
void dcache_flush(void);
void dcache_flush_area(void *addr, int size);
#else
#define dcache_flush()
#define dcache_flush_area(addr, size)
#endif

static inline void __attribute__((always_inline))
hal_debug_break(void)
{
    __asm__ volatile (" sdbbp 0");
}

#endif /* __MCU_PIC32_H__ */
