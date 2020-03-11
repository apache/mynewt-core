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

MEMORY
{
    /* 
     * Flash is remapped at 0x0 by 1st stage bootloader, but this is done with
     * an offset derived from image header thus it is safer to use remapped
     * address space at 0x0 instead of QSPI_M address space at 0x16000000.
     * Bootloader partition is 32K, but 9K is currently reserved for product
     * header (8K) and image header (1K).
     * First 512 bytes of SYSRAM are remapped at 0x0 and used as ISR vector
     * (there's no need to reallocate ISR vector) and thus cannot be used by
     * application.
     */

    FLASH (rx)  : ORIGIN = (0x00000000), LENGTH = (23 * 1024)
    RAM (rw)    : ORIGIN = (0x20000000), LENGTH = (512 * 1024)
}

/* The bootloader does not contain an image header */
_imghdr_size = 0x0;
