#!/bin/bash 
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

if [ ! -d "$1" ]; then
    echo "Must specify a larva directory to copy from, and directory must exist" 
    exit
fi

if [ ! -d "$2" ]; then
    echo "Must specify a tadpole directory to copy to, and directory must exist" 
    exit
fi

LARVA_DIR=$1
TADPOLE_DIR=$2

declare -a DIRS=("/hw/mcu/native" "/hw/bsp/native" "/hw/hal" "/libs/os" "libs/testutil" "/compiler/sim" "/libs/util") 
for dir in "${DIRS[@]}"
do
    echo "Copying $dir"
    rm -rf "$TADPOLE_DIR/$dir"
    cp -rf "$LARVA_DIR/$dir" "$TADPOLE_DIR/$dir"
    
    find "$TADPOLE_DIR/$dir" -type d -name "bin" -prune -exec rm -rf {} \;
    find "$TADPOLE_DIR/$dir" -type d -name "obj" -prune -exec rm -rf {} \;
    find "$TADPOLE_DIR/$dir" -type f -name "*.swp" -prune -exec rm -f {} \;
done

