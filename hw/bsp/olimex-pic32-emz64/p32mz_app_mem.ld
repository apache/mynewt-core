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

/*
 * Part of the MEMORY section that defines application memory size.
 * This file is included by MCU linker script.
 * this file does not have whole MEMORY section to make it clear
 * what are differences between MCUs (Flash size and RAM), config
 * sections are same.
 */

  /* Program leave space for image header ~192KB */
  kseg0_program_mem     (rx)  : ORIGIN = 0x9D000020, LENGTH = 0x7FFE0
  kseg1_program_mem     (rx)  : ORIGIN = 0xBD000020, LENGTH = 0x7FFE0
  /* Cachable RAM 512KB */
  kseg0_data_mem       (w!x)  : ORIGIN = 0x80000000, LENGTH = 0x80000
