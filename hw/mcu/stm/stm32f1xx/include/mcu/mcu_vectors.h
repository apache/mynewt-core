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

#if defined(STM32F100xB)
#include "vectors/stm32f100xb_vectors.h"
#elif defined(STM32F100xE)
#include "vectors/stm32f100xe_vectors.h"
#elif defined(STM32F101x6)
#include "vectors/stm32f101x6_vectors.h"
#elif defined(STM32F101xB)
#include "vectors/stm32f101xb_vectors.h"
#elif defined(STM32F101xE)
#include "vectors/stm32f101xe_vectors.h"
#elif defined(STM32F101xG)
#include "vectors/stm32f101xg_vectors.h"
#elif defined(STM32F102x6)
#include "vectors/stm32f102x6_vectors.h"
#elif defined(STM32F102xB)
#include "vectors/stm32f102xb_vectors.h"
#elif defined(STM32F103x6)
#include "vectors/stm32f103x6_vectors.h"
#elif defined(STM32F103xB)
#include "vectors/stm32f103xb_vectors.h"
#elif defined(STM32F103xE)
#include "vectors/stm32f103xe_vectors.h"
#elif defined(STM32F103xG)
  #include "vectors/stm32f103xg_vectors.h"
#elif defined(STM32F105xC)
  #include "vectors/stm32f105xc_vectors.h"
#elif defined(STM32F107xC)
  #include "vectors/stm32f107xc_vectors.h"
#else
 #error "Please select first the target STM32F1xx device used in your application (in stm32f1xx.h file)"
#endif
