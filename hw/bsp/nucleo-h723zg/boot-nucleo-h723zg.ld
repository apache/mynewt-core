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

/* Linker script to configure memory regions. */
MEMORY
{
  ITCM (xrw)       : ORIGIN = 0x00000000,   LENGTH = 64K
  DTCM (xrw)       : ORIGIN = 0x20000000,   LENGTH = 128K
  FLASH (rx)       : ORIGIN = 0x08000000,   LENGTH = 128K /* Bootloader area */
  RAM  (xrw)       : ORIGIN = 0x24000000,   LENGTH = 320K /* AXI (128) + AXI/ITCM (192) */
  RAM_D2  (xrw)    : ORIGIN = 0x30000000,   LENGTH = 32K  /* SRAM1 + SRMA2 */
  RAM_D3  (xrw)    : ORIGIN = 0x38000000,   LENGTH = 16K  /* SRAM4 */
}

/* The bootloader does not contain an image header */
_imghdr_size = 0x0;
