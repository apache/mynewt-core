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

#if defined(STM32F030x6)
#include "vectors/stm32f030x6_vectors.h"
#elif defined(STM32F030x8)
#include "vectors/stm32f030x8_vectors.h"
#elif defined(STM32F031x6)
#include "vectors/stm32f031x6_vectors.h"
#elif defined(STM32F038xx)
#include "vectors/stm32f038xx_vectors.h"
#elif defined(STM32F042x6)
#include "vectors/stm32f042x6_vectors.h"
#elif defined(STM32F048xx)
#include "vectors/stm32f048xx_vectors.h"
#elif defined(STM32F051x8)
#include "vectors/stm32f051x8_vectors.h"
#elif defined(STM32F058xx)
#include "vectors/stm32f058xx_vectors.h"
#elif defined(STM32F070x6)
#include "vectors/stm32f070x6_vectors.h"
#elif defined(STM32F070xB)
#include "vectors/stm32f070xb_vectors.h"
#elif defined(STM32F071xB)
#include "vectors/stm32f071xb_vectors.h"
#elif defined(STM32F072xB)
#include "vectors/stm32f072xb_vectors.h"
#elif defined(STM32F078xx)
#include "vectors/stm32f078xx_vectors.h"
#elif defined(STM32F091xC)
#include "vectors/stm32f091xc_vectors.h"
#elif defined(STM32F098xx)
#include "vectors/stm32f098xx_vectors.h"
#elif defined(STM32F030xC)
#include "vectors/stm32f030xc_vectors.h"    
#else
#error "Please select first the target STM32F0xx device used in your application (in stm32f0xx.h file)"
#endif
