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

#
# Which build artifact should be loaded to target
#
common_file_to_load () {
    if [ "$MFG_IMAGE" ]; then
        FILE_NAME=$BIN_BASENAME.bin
    elif [ "$BOOT_LOADER" ]; then
        FILE_NAME=$BIN_BASENAME.elf.bin
    else
        FILE_NAME=$BIN_BASENAME.img
    fi
}

#
# Check if this is executing in Windows. If so, set variable WINDOWS to 1.
# Also check if $COMSPEC is set or not.
#
windows_detect() {
    WINDOWS=0
    if [ "$OS" = "Windows_NT" ]; then
	WINDOWS=1
    fi
    if [ $WINDOWS -eq 1 -a -z "$COMSPEC" ]; then
	COMSPEC=cmd.exe
    fi
}
