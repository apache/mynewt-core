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

#if defined(STM32F301x8)
#include "vectors/stm32f301x8_vectors.h"
#elif defined(STM32F302x8)
#include "vectors/stm32f302x8_vectors.h"
#elif defined(STM32F302xC)
#include "vectors/stm32f302xc_vectors.h"
#elif defined(STM32F302xE)
#include "vectors/stm32f302xe_vectors.h"
#elif defined(STM32F303x8)
#include "vectors/stm32f303x8_vectors.h"
#elif defined(STM32F303xC)
#include "vectors/stm32f303xc_vectors.h"
#elif defined(STM32F303xE)
#include "vectors/stm32f303xe_vectors.h"
#elif defined(STM32F373xC)
#include "vectors/stm32f373xc_vectors.h"
#elif defined(STM32F334x8)
#include "vectors/stm32f334x8_vectors.h"
#elif defined(STM32F318xx)
#include "vectors/stm32f318xx_vectors.h"
#elif defined(STM32F328xx)
#include "vectors/stm32f328xx_vectors.h"
#elif defined(STM32F358xx)
#include "vectors/stm32f358xx_vectors.h"
#elif defined(STM32F378xx)
#include "vectors/stm32f378xx_vectors.h"
#elif defined(STM32F398xx)
#include "vectors/stm32f398xx_vectors.h"
#else
#error "Please select first the target STM32F3xx device used in your application"
#endif
