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
  FLASH (rx) : ORIGIN = 0x01008000, LENGTH = 0x30000
  RAM (rwx) : ORIGIN = 0x21000000, LENGTH = 0x10000
  IPC (rw)  : ORIGIN = 0x20000400, LENGTH = 0x400
  sram_ipc0_tx (rw)  : ORIGIN = 0x20070000, LENGTH = 0x4000
  sram_ipc0_rx (rw)  : ORIGIN = 0x20074000, LENGTH = 0x4000
}

/* This linker script is used for images and thus contains an image header */
_imghdr_size = 0x20;
