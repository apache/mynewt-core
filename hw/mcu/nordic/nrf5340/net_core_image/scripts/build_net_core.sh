#!/bin/bash

# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

NEWT=${MYNEWT_NEWT_PATH}
OBJCOPY=${MYNEWT_OBJCOPY_PATH}
AR=${MYNEWT_AR_PATH}
NET_CORE_A=${MYNEWT_USER_SRC_DIR}/net_core_img.a
NET_CORE_O=${MYNEWT_USER_SRC_DIR}/net_core_img.o
# Default flags for create-image
CREATE_IMAGE_FLAGS="timestamp"

if [ -z "${MYNEWT_VAL_NRF5340_NET_APPLICATION_VERSION}" ] && [ -z "${MYNEWT_VAL_NRF5340_NET_APPLICATION_SIGNING_KEYS}" ]; then
  echo Using default create-image flags=$CREATE_IMAGE_FLAGS
else
  CREATE_IMAGE_FLAGS="${MYNEWT_VAL_NRF5340_NET_APPLICATION_VERSION} ${MYNEWT_VAL_NRF5340_NET_APPLICATION_SIGNING_KEYS}"
  echo Using user defined create-image flags=$CREATE_IMAGE_FLAGS
fi

export WORK_DIR=${MYNEWT_USER_WORK_DIR}

export NEWT_CREATE_IMAGE_OUTPUT=$WORK_DIR/create_image_output
pushd $MYNEWT_PROJECT_ROOT

${NEWT} create-image ${MYNEWT_VAL_NET_CORE_IMAGE_TARGET_NAME} $CREATE_IMAGE_FLAGS -o $NEWT_CREATE_IMAGE_OUTPUT
# Extract image name from output
export NET_CORE_IMG=`awk '/App image successfully generated:/ { print $NF }' $NEWT_CREATE_IMAGE_OUTPUT`

cd ${WORK_DIR}
if [ -n "$NET_CORE_IMG" ] ; then
  # Copy image to local folder so objcopy creates symbols with short names instead of very long ones.
  # Name net_core.img translates to two symbols created by objcopy:
  # _binary_net_core_img_start, _binary_net_core_img_end those symbols are then used to reference content
  # of net_core.img. Changing this name could lead to invalid builds.
  cp $NET_CORE_IMG ${WORK_DIR}/net_core.img

  # Make .o file from existing binary image
  ${OBJCOPY} --rename-section .data=.net_core_img,alloc,load,readonly,data,contents -O elf32-littlearm -B armv8-m.main -I binary net_core.img ${NET_CORE_O}

  # Archive .o for newt to use
  ${AR} -rcs ${NET_CORE_A} ${NET_CORE_O}
fi
popd
