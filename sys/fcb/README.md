<!--
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
-->

# Flash circular buffer

# Overview

Storage of elements in flash in FIFO fashion. Elements are appended to the of the area until storage space is exhausted. Then the oldest sector should be erased and that can be used in storing new entries.

# API

fcb_init()
  - initialize fcb for a given array of flash sectors

fcb_append()
  - reserve space to store an element
fcb_append_finish()
  - storage of the element is finished; can calculate CRC for it

fcb_walk(cb, sector)
  - call cb for every element in the buffer. Or for every element in
    a particular flash sector, if sector is specified
fcb_getnext(elem)
  - return element following elem

fcb_rotate()
  - erase oldest used sector, and make it current

# Usage

To add an element to circular buffer:
1. call fcb_append() to get location; if this fails due to lack of space,
   call fcb_rotate()
2. use flash_area_write() to write contents
3. call fcb_append_finish() when done

To read contents of the circular buffer:
1. call fcb_walk() with callback
2. within callback: copy in data from the element using flash_area_read(),
   call fcb_rotate() when all elements from a given sector have been read
