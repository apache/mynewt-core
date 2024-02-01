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

#if defined(NRF52805_XXAA)
#include "vectors/nrf52805_vectors.h"
#elif defined(NRF52810_XXAA)
#include "vectors/nrf52810_vectors.h"
#elif defined(NRF52811_XXAA)
#include "vectors/nrf52811_vectors.h"
#elif defined(NRF52820_XXAA)
#include "vectors/nrf52820_vectors.h"
#elif defined(NRF52832_XXAA) || defined (NRF52832_XXAB)
#include "vectors/nrf52_vectors.h"
#elif defined(NRF52833_XXAA)
#include "vectors/nrf52833_vectors.h"
#elif defined(NRF52840_XXAA)
#include "vectors/nrf52840_vectors.h"
#else
#error "Unsupported device"
#endif /* NRF51, NRF52805_XXAA, NRF52810_XXAA, NRF52811_XXAA, NRF52820_XXAA, NRF52832_XXAA, NRF52832_XXAB, NRF52833_XXAA, NRF52840_XXAA, NRF5340_XXAA_APPLICATION, NRF5340_XXAA_NETWORK, NRF9160_XXAA */
