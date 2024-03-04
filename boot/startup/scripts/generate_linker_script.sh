#! /bin/bash
#
# SPDX-License-Identifier: Apache-2.0

cd ${MYNEWT_PROJECT_ROOT}

TARGET_NAME=`echo ${MYNEWT_VAL_TARGET_NAME} | sed 's/"//g'`
ARCH=`echo ${MYNEWT_VAL_ARCH_NAME} | sed  's/"//g'`
MCU_PATH=`echo ${MYNEWT_VAL_MCU_PATH} | sed  's/"//g'`
LINK_TEMPLATE=`echo ${MYNEWT_VAL_LINK_TEMPLATE} | sed  's/"//g'`
if [ -z "$LINK_TEMPLATE" ] ; then
  LINK_TEMPLATE=repos/apache-mynewt-core/boot/startup/mynewt_${ARCH}.ld
fi
LINK_TEMPLATE=`echo $LINK_TEMPLATE`
# Create line and link/include directories
# link/mynewt.ld will be final linker script
# link/include/ will have empty files that could be included during mynewt.ld creation if
# files with the same name are not present in other places
mkdir -p ${MYNEWT_BUILD_GENERATED_DIR}/link
mkdir -p ${MYNEWT_BUILD_GENERATED_DIR}/link/include
if [ ! -f ${MYNEWT_BUILD_GENERATED_DIR}/link/include/link_tables.ld.h ] ; then
  touch ${MYNEWT_BUILD_GENERATED_DIR}/link/include/link_tables.ld.h
fi
touch ${MYNEWT_BUILD_GENERATED_DIR}/link/include/target_config.ld.h
touch ${MYNEWT_BUILD_GENERATED_DIR}/link/include/memory_regions.ld.h
touch ${MYNEWT_BUILD_GENERATED_DIR}/link/include/bsp_config.ld.h
touch ${MYNEWT_BUILD_GENERATED_DIR}/link/include/mcu_config.ld.h

set >env.txt
${MYNEWT_CC_PATH} -xc -DMYNEWT_SYSFLASH_ONLY_CONST -P -E \
  -I${MYNEWT_TARGET_PATH}/link/include \
  -I${MYNEWT_APP_PATH}/link/include \
  -I${BSP_PATH}/link/include \
  -I${MCU_PATH}/link/include \
  -I${MYNEWT_BUILD_GENERATED_DIR}/include \
  -Irepos/apache-mynewt-core/boot/startup/include \
  -I${MYNEWT_BUILD_GENERATED_DIR}/link/include \
  $LINK_TEMPLATE \
  >${MYNEWT_BUILD_GENERATED_DIR}/link/mynewt.ld

