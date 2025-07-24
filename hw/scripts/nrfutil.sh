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
. $CORE_PATH/hw/scripts/common.sh

jlink_sn () {
    if [ -n "$JLINK_SN" ]; then
        SRN_ARG="--serial-number $JLINK_SN"
    elif [ -n "$MYNEWT_VAL_JLINK_SN" ]; then
        SRN_ARG="--serial-number $MYNEWT_VAL_JLINK_SN"
    else
        SRN_ARG=""
    fi
}

#
# FILE_NAME must contain the name of the file to load
# FLASH_OFFSET must contain the offset in flash where to place it
#
nrfutil_load () {
    HEX_FILE=${BIN_BASENAME}.hex
    ELF_FILE=${BIN_BASENAME}.elf
    if [ -z ${FILE_NAME} ]; then
        echo "Missing filename"
        exit 1
    fi
    # If nordicDfu is to be used, create hex file directly from ELF
    if [ "$MYNEWT_VAL_NRFUTIL_TRAITS" == "nordicDfu" ] ; then
        arm-none-eabi-objcopy -O ihex ${ELF_FILE} ${HEX_FILE}
    elif [ ! -f "${FILE_NAME}" ]; then
        # tries stripping current path for readability
        FILE=${FILE_NAME##$(pwd)/}
        echo "Cannot find file" $FILE
        exit 1
    elif [ -z ${FLASH_OFFSET} ]; then
        echo "Missing flash offset"
        exit 1
    else
        arm-none-eabi-objcopy -O ihex -I binary --adjust-vma ${FLASH_OFFSET} ${FILE_NAME} ${HEX_FILE}
    fi

    if [ -n "${MYNEWT_VAL_NRFJPROG_COPROCESSOR}" ] ; then
      case ${MYNEWT_VAL_NRFJPROG_COPROCESSOR} in
        "CP_NETWORK")
          NRFUTIL_ARG="${NRFUTIL_ARG} --core Network"
          ;;
        *)
          NRFUTIL_ARG="${NRFUTIL_ARG} --core Application"
          ;;
      esac
    fi

    ret=0
    if [ -z ${NRFUTIL_TRAITS} ] ; then
        if [ -z ${MYNEWT_VAL_NRFUTIL_TRAITS} ] ; then
            NRFUTIL_TRAITS="--traits jlink"
        else
            NRFUTIL_TRAITS="--traits ${MYNEWT_VAL_NRFUTIL_TRAITS}"
            if [ $MYNEWT_VAL_NRFUTIL_TRAITS == "nordicDfu" ] ; then
                ZIP_FILE=${BIN_BASENAME}.zip
                PORT=`nrfutil device list --traits nordicDfu | awk '/ports/ { print $2 }'`
                # TODO: hw-version is probably incorrect for non NRF52 devices
                nrfutil pkg generate --hw-version 52 --sd-req 0 --application ${HEX_FILE} --application-version 1 ${ZIP_FILE}
                echo "Downloading" ${ZIP_FILE}
                nrfutil dfu usb-serial -p ${PORT} --package ${ZIP_FILE}
                if [ $? -ne 0 ]; then
                    ret=1
                fi
            fi
        fi
    fi

    if [ -z ${ZIP_FILE} ] ; then
        jlink_sn

        echo "Downloading" ${HEX_FILE}

        nrfutil device program --firmware ${HEX_FILE} $SRN_ARG ${NRFUTIL_ARG} ${NRFUTIL_TRAITS} --options chip_erase_mode=ERASE_RANGES_TOUCHED_BY_FIRMWARE

        if [ $? -ne 0 ]; then
            ret=1
        else
            nrfutil device reset $SRN_ARG
        fi
    fi

    return $ret
}
