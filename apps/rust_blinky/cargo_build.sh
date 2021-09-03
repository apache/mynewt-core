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

# Map mynewt architecture to cargo target
if [[ ${MYNEWT_VAL_ARCH_NAME} == '"cortex_m0"' ]]; then
  TARGET="thumbv6m-none-eabi"
elif [[ ${MYNEWT_VAL_ARCH_NAME} == '"cortex_m3"' ]]; then
  TARGET="thumbv7m-none-eabi"
elif [[ ${MYNEWT_VAL_ARCH_NAME} == '"cortex_m33"' ]]; then
  if [[ $MYNEWT_VAL_HARDFLOAT -eq 1 ]]; then
    TARGET="thumbv8m.main-none-eabihf"
  else
    TARGET="thumbv8m.main-none-eabih"
  fi
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

# Map mynewt build profile to cargo build profile
if [[ ${MYNEWT_BUILD_PROFILE} == 'debug' ]]; then
  # mynewt debug profile -> cargo debug profile
  CARGO_PROFILE="debug"
  CARGO_ARGS=""
else
  # all other mynewt profiles -> cargo release profile
  CARGO_PROFILE="release"
  CARGO_ARGS="--release"
fi

cargo build --target="${TARGET}" --target-dir="${MYNEWT_PKG_BIN_DIR}" ${CARGO_ARGS}
cp "${MYNEWT_PKG_BIN_DIR}"/${TARGET}/${CARGO_PROFILE}/*.a "${MYNEWT_PKG_BIN_ARCHIVE}"
