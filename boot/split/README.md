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

`libs/split` is a library required to build split applications.  When building a split application you must include `libs/split` in your loader and your application

## Split Image

Split applications allow the user to build the application content separate from the library content by splitting an application into two pieces:

* A "loader" which contains a separate application that can perform upgrades and manage split images
* A "split app" which contains the main application content and references the libraries in the loader by static linkage

See [split image architecture](http://mynewt.apache.org/latest/os/modules/split/split/) for the details of split image design.


## Contents

`libs/split` contains the following components

* The split app configuration which tells the system whether to run the "loader" or the "app"
* The newrmgr commands to access split functionality
* The functions used by the loader to check and run a split application

## Examples

### Split App

Your split application and loader must initialize the library by calling

```
#include "split/split.h"
void split_app_init(void);
```

This registers the configuration and commands for split applications.

### Loader

Your loader can call 

```
int split_app_go(void **entry, int toBoot);
```

to check whether the split application can be run.  A example is shown below

```
#include "split/split.h"
#include "bootutil/bootutil.h"
    {
        void *entry;
        rc = split_app_go(&entry, true);
        if(rc == 0) {
            system_start(entry);
        }
    }
```



