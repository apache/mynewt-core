/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#if defined(NRF51)
#include <mcu/nrf51_vectors.h>
#elif defined(NRF52805_XXAA)
#include <mcu/nrf52805_vectors.h>
#elif defined(NRF52810_XXAA)
#include <mcu/nrf52810_vectors.h>
#elif defined(NRF52811_XXAA)
#include <mcu/nrf52811_vectors.h>
#elif defined(NRF52820_XXAA)
#include <mcu/nrf52820_vectors.h>
#elif defined(NRF52832_XXAA) || defined (NRF52832_XXAB)
#include <mcu/nrf52832_vectors.h>
#elif defined(NRF52833_XXAA)
#include <mcu/nrf52833_vectors.h>
#elif defined(NRF52840_XXAA)
#include <mcu/nrf52840_vectors.h>
#elif defined(NRF5340_XXAA_APPLICATION)
#include <mcu/nrf5340_application_vectors.h>
#elif defined(NRF5340_XXAA_NETWORK)
    #include <mcu/nrf5340_network_vectors.h>
#elif defined(NRF9120_XXAA) || defined(NRF9160_XXAA)
    #include <mcu/nrf91_vectors.h>
#else
    #error "Unknown device."
#endif
