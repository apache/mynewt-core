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


<img src="http://mynewt.apache.org/img/logo.svg" width="250" alt="Apache Mynewt">

## Overview

`apps/splitty` is an example split application.  It can be paired with slinky to form an application.

## Split Image

Split applications allow the user to build the application content separate from the library content by splitting an application into two pieces:

* A "loader" which contains a separate application that can perform upgrades and manage split images
* A "split app" which contains the main application content and references the libraries in the loader by static linkage

See [split image architecture](http://mynewt.apache.org/latest/os/modules/split/split/) for the details of split image design.

## Contents

splitty is just a simply app that has newtmgr, shell and blinkys the LED at a differernt rate than slinky, so its visually evident that its running.

## Usage

You can use splitty as part of a split app by setting up your target.

```
targets/app
    app=@apache-mynewt-core/apps/splitty
    loader=@apache-mynewt-core/apps/slinky
    bsp=@apache-mynewt-core/hw/bsp/nordic_pca10040
    build_profile=optimized
```



