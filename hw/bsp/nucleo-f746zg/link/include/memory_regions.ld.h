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

/* Fragment that goes to MEMORY section */
#ifndef SECTIONS_REGIONS

#ifdef STACK_REGION
    DTCM (rwx) :  ORIGIN = 0x20000000, LENGTH = (64K - STACK_SIZE)
    STACK_RAM (rw) : ORIGIN = 0x20020000 - STACK_SIZE, LENGTH = STACK_SIZE
#else
    DTCM (rwx) :  ORIGIN = 0x20000000, LENGTH = 64K
#endif
    ITCM (rx)  :  ORIGIN = 0x00000000, LENGTH = 16K

#else
/* Fragment that goes into SECTIONS, can provide definition and sections if needed */
    _itcm_start = ORIGIN(ITCM);
    _itcm_end = ORIGIN(ITCM) + LENGTH(ITCM);
    _dtcm_start = ORIGIN(DTCM);
    _dtcm_end = ORIGIN(DTCM) + LENGTH(DTCM);

#endif
