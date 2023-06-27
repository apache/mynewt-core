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

# Try to detect connected programmers
detect_programmer() {

    DETECTED_PROGRAMMER='none'
    
    # check if lsusb command is available
    if [ $(which lsusb) ] ; then

        # extract the VID:PID list for connected USB devices
        USB_DEV=$(lsusb | cut -f6 -d' ')

    elif [ $(which system_profiler) ] ; then

        # extract the VID:PID list for connected USB devices on OSX
        # and get them in the same format as lsusb from Linux
        USB_DEV=$(system_profiler SPUSBDataType 2>/dev/null | awk '
            /^[ \t]+Product ID:/ {
                # remove leading 0x from hex number
                sub(/^0x/, "", $3);
                PID=$3;
            }
            /^[ \t]+Vendor ID:/ {
                # remove leading 0x from hex number
                sub(/^0x/, "", $3);
                printf("%s:%s\n", PID, $3);
            }
        ')

    fi

    if [ -n "$USB_DEV" ]; then

        echo "$USB_DEV" | grep -q -i 'c251:f001'
        [ $? -eq 0 ] && DETECTED_PROGRAMMER='cmsis-dap'

        echo "$USB_DEV" | grep -q -i '0483:3748'
        [ $? -eq 0 ] && DETECTED_PROGRAMMER='stlink-v2'

        echo "$USB_DEV" | grep -q -i '0483:374b'
        [ $? -eq 0 ] && DETECTED_PROGRAMMER='stlink-v2-1'

        echo "$USB_DEV" | grep -q -i '1366:1015'
        [ $? -eq 0 ] && DETECTED_PROGRAMMER='jlink'

    fi

    echo "Detected programmer: $DETECTED_PROGRAMMER"
}

gdb_detect() {
    # GDB setup previously, no need to detect
    if [ -n "$GDB" ] ; then return ; fi

    GDB=gdb

    case ${MYNEWT_VAL_ARCH_NAME} in
        \"cortex*\")
            if which arm-none-eabi-gdb >/dev/null 2>/dev/null ; then
                GDB=arm-none-eabi-gdb
            elif which gdb-multiarch >/dev/null 2>/dev/null ; then
                GDB=gdb-multiarch
            fi
            ;;
        \"rv32*\")
            if which riscv64-unknown-elf-gdb >/dev/null 2>/dev/null ; then
                GDB=riscv64-unknown-elf-gdb
            elif which riscv-none-embed-gdb >/dev/null 2>/dev/null ; then
                GDB=riscv-none-embed-gdb
            elif which gdb-multiarch >/dev/null 2>/dev/null ; then
                GDB=gdb-multiarch
            fi
            ;;
        *)
            if which gdb-multiarch >/dev/null 2>/dev/null ; then
                GDB=gdb-multiarch
            fi
            ;;
    esac
}
