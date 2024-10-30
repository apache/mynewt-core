<!--
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
-->

# TFM package

# Overview

This package provides way to connect secure code from bootloader with
non-secure code of the application.

Bootloader build with `MCU_APP_SECURE` set to 1 will mark non-bootloader
flash as non-secure and part for the bootloader flash region as
non-secure callable.

When syscfg has `TFM_EXPORT_NSC: 1`, during build of the bootloader, additional library `tfm_s_CMSE_lib.a`
will be created that can be linked to the application code so secure functions ar callable.

Application build as unsecure should specify `TFM_IMPORT_NSC: 1` to mark function as non-secure-callable.
Easy connection between the bootloader and the application is achieved with syscfg value `TFM_SECURE_BOOT_TARGET`
that can point to bootloader target where import library should be generated.

# Examples targets

### 1. Bootloader target

#### targets/nordic_pca10095-boot_sec/pkg.yml
```yaml
pkg.name: "targets/nordic_pca10095-boot_sec"
pkg.type: target
````
#### targets/nordic_pca10095-boot_sec/target.yml
```yaml
target.app: "@mcuboot/boot/mynewt"
target.bsp: "@apache-mynewt-core/hw/bsp/nordic_pca10095"
````
#### targets/nordic_pca10095-boot_sec/syscfg.yml
```yaml
syscfg.vals:
    # Build for non-secure application
    # Bootloader is still secure
    MCU_APP_SECURE: 0

    # Export NSC functions to library
    TFM_EXPORT_NSC: 1
````
### 2. Application target

#### targets/nordic_pca10095-btshell/pkg.yml
```yaml
pkg.name: "targets/nordic_pca10095-btshell"
pkg.type: target
````
#### targets/nordic_pca10095-btshell/target.yml
```yaml
target.app: "@apache-mynewt-nimble/apps/btshell"
target.bsp: "@apache-mynewt-core/hw/bsp/nordic_pca10095"
````
#### targets/nordic_pca10095-btshell/syscfg.yml
```yaml
syscfg.vals:
    # Build application as non-secure code
    MCU_APP_SECURE: 0
    # Enable net core automatically
    BSP_NRF5340_NET_ENABLE: 1
    # Embed default blehci target inside application image
    NRF5340_EMBED_NET_CORE: 1
    
    # Inform that NSC functions should be imported from library 
    TFM_IMPORT_NSC: 1
    # Set booloader target that should produce library with NSC functions
    TFM_SECURE_BOOT_TARGET: nordic_pca10095-boot_sec
````
