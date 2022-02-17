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
# da1469x_serial.py to reset the device using JLink

PORT=3333
JLINK_GDB_SERVER=JLinkGDBServer
OUT_INT=reset_int.log
OUT=reset.log

ip=${1:-"localhost"}
freq=${2:-"1000"}
GDB_CMD_FILE=${GDB_CMD_FILE:-".gdb_load"}
CFG_FILE_DIR=${CFG_FILE_DIR:-"./cfg"}
JLINK_LOG_FILE=${JLINK_LOG_FILE:-".jlink_log"}

if [ -z $(which arm-none-eabi-gdb) ]; then
  GDB=gdb
else
  GDB=arm-none-eabi-gdb
fi

cat > ${GDB_CMD_FILE} << EOF
set pagination off
EOF

cat >> ${GDB_CMD_FILE} << EOF
shell sh -c "trap '' 2; $JLINK_GDB_SERVER -device cortex-m33 -speed 4000 -if SWD -port $PORT -singlerun &"
EOF

# generate HW RESET REQ
cat >> ${GDB_CMD_FILE} << EOF
target remote ${ip}:$PORT
mon reset
set *(int *)0x100C0050 = 1
q
EOF

$GDB -batch -x ${GDB_CMD_FILE} > $OUT_INT $ 2>&1 > $OUT
if [ $? != 0 ]; then
  exit 1
fi
rm $OUT_INT

sleep 1
exit 0
