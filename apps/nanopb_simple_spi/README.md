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

### Nanopb SPI Simple Example

##### Overview
********

This application is based on Nanopb example called Simple.
It contains source and header file generated with Nanopb.
The header file defines one structure (message) with one int field
called lucky_number. The application shows how to use basic Nanopb API and
configures the SPI to send the message and receive it on the other device.
Two devices are required for this to work:
1. MASTER - encodes and sends one message per second over the SPI.
   After sending one message the lucky_number is incremented, encoded
   and sent again. Toggles the LED after each sent message.
2. SLAVE - after receiving the message it decodes it and prints
   the lucky_number value. Toggles the LED after each received
   message.
