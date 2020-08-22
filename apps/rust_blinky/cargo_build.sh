#!/bin/bash
set -eu

#
# Copyright 2020 Casper Meijn <casper@meijn.net>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

if [[ ${MYNEWT_VAL_ARCH_NAME} == '"cortex_m0"' ]]; then
  TARGET="thumbv6m-none-eabi"
elif [[ ${MYNEWT_VAL_ARCH_NAME} == '"cortex_m3"' ]]; then
  TARGET="thumbv7m-none-eabi"
elif [[ ${MYNEWT_VAL_ARCH_NAME} == '"cortex_m4"' || ${MYNEWT_VAL_ARCH_NAME} == '"cortex_m7"' ]]; then
  if [[ $MYNEWT_VAL_HARDFLOAT -eq 1 ]]; then
    TARGET="thumbv7em-none-eabihf"
  else
    TARGET="thumbv7em-none-eabi"
  fi
else
  echo "The ARCH_NAME ${MYNEWT_VAL_ARCH_NAME} is not supported"
  exit 1
fi

cargo build --target="${TARGET}" --target-dir="${MYNEWT_PKG_BIN_DIR}"
cp "${MYNEWT_PKG_BIN_DIR}"/${TARGET}/debug/*.a "${MYNEWT_PKG_BIN_ARCHIVE}"
