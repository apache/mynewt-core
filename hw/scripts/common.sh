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

#
# Check if known parameters are set in EXTRA_JTAG_CMD. Filter these
# parameters out of the arguments passed to jtag emulator software.
# Only special parameter is -port <portnumber>
#
parse_extra_jtag_cmd() {
    PORT=3333

    NEW_EXTRA_JTAG_CMD=""
    while [ "$1" != "" ]; do
        case $1 in
            -port)
                shift
                # Many BSP scripts append their own things additional
                # parameters. This is done in a way where openocd delimeter is
                # immediatelly adjacent to parameters passed via newt.
                # The following is to filter out the delimeter from
                # PORT, but keep it present within the string passed
                # to openocd.
                PORT=`echo $1 | tr -c -d 0-9`
                ADDITIONAL_CHARS=`echo $1 | tr -d 0-9`
                NEW_EXTRA_JTAG_CMD="$NEW_EXTRA_JTAG_CMD $ADDITIONAL_CHARS"
                shift
                ;;
            *)
                NEW_EXTRA_JTAG_CMD="$NEW_EXTRA_JTAG_CMD $1"
                shift
                ;;
        esac
    done
    echo $NEW_EXTRA_JTAG_CMD
    EXTRA_JTAG_CMD=$NEW_EXTRA_JTAG_CMD
}
