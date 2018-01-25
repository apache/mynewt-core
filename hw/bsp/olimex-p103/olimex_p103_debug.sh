#!/bin/sh
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# 
#   http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

# Called with following variables set:
#  - CORE_PATH is absolute path to @apache-mynewt-core
#  - BSP_PATH is absolute path to hw/bsp/bsp_name
#  - BIN_BASENAME is the path to prefix to target binary,
#    .elf appended to name is the ELF file
#  - FEATURES holds the target features string
#  - EXTRA_JTAG_CMD holds extra parameters to pass to jtag software
#  - RESET set if target should be reset when attaching
#  - NO_GDB set if we should not start gdb to debug
#
. $CORE_PATH/hw/scripts/openocd.sh

FILE_NAME=$BIN_BASENAME.elf
CFG="-s $BSP_PATH -f $BSP_PATH/olimex_p103.cfg"
# Exit openocd when gdb detaches.
EXTRA_JTAG_CMD="$EXTRA_JTAG_CMD; stm32f1x.cpu configure -event gdb-detach {if {[stm32f1x.cpu curstate] eq \"halted\"} resume;shutdown}"

openocd_debug
