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

# loramac-node

This is a Mynewt port of the Semtech LoRaWAN endpoint stack.  This package depends on two drivers:
    * loramac-net radio - Facilitates communication with a particular type of LoRa modem.  This package exports the Radio_s definition containing the function pointers required by this stack.
    * loramac-net board - Contains MCU or BSP specific definition needed for handling interrupts triggered by the LoRa radio.

Sample drivers of both types can be found at: @apache-mynewt-core/hw/drivers/loramc-node.
