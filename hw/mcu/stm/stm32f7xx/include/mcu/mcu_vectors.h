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

#if defined(STM32F722xx)
#include "vectors/stm32f722xx_vectors.h"
#elif defined(STM32F723xx)
#include "vectors/stm32f723xx_vectors.h"
#elif defined(STM32F732xx)
#include "vectors/stm32f732xx_vectors.h"
#elif defined(STM32F733xx)
#include "vectors/stm32f733xx_vectors.h"
#elif defined(STM32F756xx)
#include "vectors/stm32f756xx_vectors.h"
#elif defined(STM32F746xx)
#include "vectors/stm32f746xx_vectors.h"
#elif defined(STM32F745xx)
#include "vectors/stm32f745xx_vectors.h"
#elif defined(STM32F765xx)
#include "vectors/stm32f765xx_vectors.h"
#elif defined(STM32F767xx)
#include "vectors/stm32f767xx_vectors.h"
#elif defined(STM32F769xx)
#include "vectors/stm32f769xx_vectors.h"
#elif defined(STM32F777xx)
#include "vectors/stm32f777xx_vectors.h"
#elif defined(STM32F779xx)
#include "vectors/stm32f779xx_vectors.h"
#elif defined(STM32F730xx)
#include "vectors/stm32f730xx_vectors.h"
#elif defined(STM32F750xx)
#include "vectors/stm32f750xx_vectors.h"
#else
#error "Please select first the target STM32F7xx device used in your application"
#endif
