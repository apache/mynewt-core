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
    elif [ -n "$MYNEWT_VAL_MYNEWT_DEBUGGER_SN" ]; then
        SRN_ARG="--serial-number $MYNEWT_VAL_MYNEWT_DEBUGGER_SN"
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

    NRF_TRAITS_OPT=""
    if [ -n "${MYNEWT_VAL_NRFUTIL_TRAITS}" ]; then
        NRF_TRAITS_OPT="--traits ${MYNEWT_VAL_NRFUTIL_TRAITS}"
    fi

    # If nordicDfu is to be used, create hex file directly from ELF
    if [ "${MYNEWT_VAL_NRFUTIL_DFU_MODE:-0}" -eq 1 ] ; then
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
    if [ "${MYNEWT_VAL_NRFUTIL_DFU_MODE:-0}" -eq 1 ] ; then
        ZIP_FILE=${BIN_BASENAME}.zip
        if [ -n "${MYNEWT_VAL_NRFUTIL_DFU_SN}" ] ; then
            PORT=$(nrfutil device list --traits nordicDfu | awk -v dfu_sn=$MYNEWT_VAL_NRFUTIL_DFU_SN \
            'dfu_sn==$1 { sn_match=1 }
             /^Ports/ { if(sn_match) { print $2; sn_match=0 }}
             ')
            if [ -z "$PORT" ] ; then
              echo "Error: NRFUTIL_DFU_SN does not match any connected board."
              return 1
            fi
        else
            PORT=$(nrfutil device list --traits nordicDfu | awk '/^Ports/ { print $2 }')
            PORT_COUNT=$(echo "$PORT" | wc -w)
            if [ "$PORT_COUNT" -eq 0 ]; then
                echo "Error: No Nordic DFU devices found."
                return 1
            elif [ "$PORT_COUNT" -gt 1 ]; then
                echo "Error: Found multiple DFU devices. Connect exactly one supported device or add NRFUTIL_DFU_SN to your target."
                return 1
            fi
        fi

        # TODO: hw-version is probably incorrect for non NRF52 devices
        nrfutil pkg generate --hw-version 52 --sd-req 0 --application ${HEX_FILE} --application-version 1 ${ZIP_FILE}
        echo "Downloading" ${ZIP_FILE}
        nrfutil dfu usb-serial -p ${PORT} --package ${ZIP_FILE}
        if [ $? -ne 0 ]; then
            ret=1
        fi
    fi

    if [ -z ${ZIP_FILE} ] ; then
        jlink_sn

        echo "Downloading" ${HEX_FILE}
        nrfutil device program --firmware ${HEX_FILE} ${SRN_ARG} ${NRF_TRAITS_OPT} ${NRFUTIL_ARG} --options chip_erase_mode=ERASE_RANGES_TOUCHED_BY_FIRMWARE

        if [ $? -ne 0 ]; then
            ret=1
        else
            nrfutil device reset $SRN_ARG ${NRF_TRAITS_OPT}
        fi
    fi

    return $ret
}
