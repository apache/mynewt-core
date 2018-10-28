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
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
/* _estack = 0x20001FFF; */   /* end of RAM */

/* Generate a link error if heap and stack don't fit into RAM */
/* _Min_Heap_Size = 0x200; */      /* required amount of heap  */
/*_Min_Stack_Size = 0x400; */ /* required amount of stack */

/* Specify the memory areas */
MEMORY
{
    FLASH (rx)      : ORIGIN = 0x8000000, LENGTH = 64K
    RAM   (rw)     : ORIGIN = 0x20000000, LENGTH = 8K
}


/*
 * Image header size - no bootloader support, no header.
 */
_imghdr_size = 0x00;
