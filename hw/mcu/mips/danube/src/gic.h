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

#ifndef H_GIC_H_
#define H_GIC_H_

#include <stdint.h>

void gic_interrupt_set(uint32_t n);
void gic_interrupt_reset(uint32_t n);
void gic_interrupt_active_high(uint32_t n);
void gic_interrupt_active_low(uint32_t n);
int gic_interrupt_is_enabled(uint32_t n);
int gic_interrupt_poll(uint32_t n);
void gic_map(int int_no, uint8_t vpe, uint8_t pin);
void gic_unmap(int int_no, uint8_t pin);
int gic_init(void);

#endif
