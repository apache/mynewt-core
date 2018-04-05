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

# Console

There are three versions of this library;
  * minimal - contains an implementation which allows for output, optional
    input, supports UART and RTT, and has support for `newtmgr` protocol.
  * full - contains all minimal features and adds formatted output through
    `console_printf`, console editing and BLE monitor.
  * stub - has stubs for the API.

You can write a package which uses ```console_printf()```, and builder of a
project can select which one they'll use.
For the package, list in the pkg.yml console as the required capability.
Project builder will then include either sys/console/full or
sys/console/minimal or sys/console/stub as their choice.

