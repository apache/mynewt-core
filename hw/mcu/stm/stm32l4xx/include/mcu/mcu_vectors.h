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

#if defined(STM32L412xx)
#include "vectors/stm32l412xx_vectors.h"
#elif defined(STM32L422xx)
#include "vectors/stm32l422xx_vectors.h"
#elif defined(STM32L431xx)
#include "vectors/stm32l431xx_vectors.h"
#elif defined(STM32L432xx)
#include "vectors/stm32l432xx_vectors.h"
#elif defined(STM32L433xx)
#include "vectors/stm32l433xx_vectors.h"
#elif defined(STM32L442xx)
#include "vectors/stm32l442xx_vectors.h"
#elif defined(STM32L443xx)
#include "vectors/stm32l443xx_vectors.h"
#elif defined(STM32L451xx)
#include "vectors/stm32l451xx_vectors.h"
#elif defined(STM32L452xx)
#include "vectors/stm32l452xx_vectors.h"
#elif defined(STM32L462xx)
#include "vectors/stm32l462xx_vectors.h"
#elif defined(STM32L471xx)
#include "vectors/stm32l471xx_vectors.h"
#elif defined(STM32L475xx)
#include "vectors/stm32l475xx_vectors.h"
#elif defined(STM32L476xx)
#include "vectors/stm32l476xx_vectors.h"
#elif defined(STM32L485xx)
#include "vectors/stm32l485xx_vectors.h"
#elif defined(STM32L486xx)
#include "vectors/stm32l486xx_vectors.h"
#elif defined(STM32L496xx)
#include "vectors/stm32l496xx_vectors.h"
#elif defined(STM32L4A6xx)
#include "vectors/stm32l4a6xx_vectors.h"
#elif defined(STM32L4P5xx)
#include "vectors/stm32l4p5xx_vectors.h"
#elif defined(STM32L4Q5xx)
#include "vectors/stm32l4q5xx_vectors.h"
#elif defined(STM32L4R5xx)
#include "vectors/stm32l4r5xx_vectors.h"
#elif defined(STM32L4R7xx)
#include "vectors/stm32l4r7xx_vectors.h"
#elif defined(STM32L4R9xx)
#include "vectors/stm32l4r9xx_vectors.h"
#elif defined(STM32L4S5xx)
#include "vectors/stm32l4s5xx_vectors.h"
#elif defined(STM32L4S7xx)
#include "vectors/stm32l4s7xx_vectors.h"
#elif defined(STM32L4S9xx)
#include "vectors/stm32l4s9xx_vectors.h"
#else
#error "Please select first the target STM32L4xx device used in your application"
#endif
