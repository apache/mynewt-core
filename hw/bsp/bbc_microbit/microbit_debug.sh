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

USE_OPENOCD=1
FILE_NAME=$BIN_BASENAME.elf

# Look for 'JLINK_DEBUG' in FEATURES
for feature in $FEATURES; do
    if [ $feature = "JLINK_DEBUG" ]; then
        USE_OPENOCD=0
    fi
done

if [ $USE_OPENOCD -eq 1 ]; then
    . $CORE_PATH/hw/scripts/openocd.sh

    CFG="-f interface/cmsis-dap.cfg -f target/nrf51.cfg"
    # Exit openocd when gdb detaches.
    EXTRA_JTAG_CMD="$EXTRA_JTAG_CMD; nrf51.cpu configure -event gdb-detach {if {[nrf51.cpu curstate] eq \"halted\"} resume;shutdown}"

    openocd_debug
else
    . $CORE_PATH/hw/scripts/jlink.sh

    JLINK_DEV="nRF51422_xxAC"

    jlink_debug
fi
