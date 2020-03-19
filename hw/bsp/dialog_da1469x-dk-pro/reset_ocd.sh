#!/usr/bin/env bash
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

# This is an independent script that gets invoked directly by
# da1469x_serial.py to reset the device using OpenOCD

ip=${1:-"localhost"}
freq=${2:-"1000"}

if [ -z $(which arm-none-eabi-gdb) ]; then
  GDB=gdb
else
  GDB=arm-none-eabi-gdb
fi

GDB_CMD_FILE=${GDB_CMD_FILE:-".gdb_load"}
OPENOCD_LOG_FILE=${OPENOCD_LOG_FILE:-".openocd_log"}
CFG_FILE_DIR=${CFG_FILE_DIR:-"./cfg"}

cat > ${GDB_CMD_FILE} << EOF
set pagination off
EOF

cat >> ${GDB_CMD_FILE} << EOF
shell sh -c "trap '' 2; openocd -f ${CFG_FILE_DIR}/ft4232h.cfg -f ${CFG_FILE_DIR}/device.cfg -d > ${OPENOCD_LOG_FILE} 2>&1 &"
EOF

cat >> ${GDB_CMD_FILE} << EOF
target remote ${ip}:3333
mon adapter_khz ${freq}
mon reset halt
mon resume

set *(int *)0x100C0050 = 1
mon shutdown

quit
EOF

$GDB -batch -x ${GDB_CMD_FILE}
if [ $? != 0 ]; then
  killall openocd
  exit 1
fi

killall openocd
exit 0 # Exit success even if killall fails

