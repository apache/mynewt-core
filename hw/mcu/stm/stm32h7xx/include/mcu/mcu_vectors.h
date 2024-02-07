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

#if defined(STM32H743xx)
#include "vectors/stm32h743xx_vectors.h"
#elif defined(STM32H753xx)
#include "vectors/stm32h753xx_vectors.h"
#elif defined(STM32H750xx)
#include "vectors/stm32h750xx_vectors.h"
#elif defined(STM32H742xx)
#include "vectors/stm32h742xx_vectors.h"
#elif defined(STM32H745xx)
#include "vectors/stm32h745xx_vectors.h"
#elif defined(STM32H745xG)
#include "vectors/stm32h745xg_vectors.h"
#elif defined(STM32H755xx)
#include "vectors/stm32h755xx_vectors.h"
#elif defined(STM32H747xx)
#include "vectors/stm32h747xx_vectors.h"
#elif defined(STM32H747xG)
#include "vectors/stm32h747xg_vectors.h"
#elif defined(STM32H757xx)
#include "vectors/stm32h757xx_vectors.h"
#elif defined(STM32H7B0xx)
#include "vectors/stm32h7b0xx_vectors.h"
#elif defined(STM32H7B0xxQ)
#include "vectors/stm32h7b0xxq_vectors.h"
#elif defined(STM32H7A3xx)
#include "vectors/stm32h7a3xx_vectors.h"
#elif defined(STM32H7B3xx)
#include "vectors/stm32h7b3xx_vectors.h"
#elif defined(STM32H7A3xxQ)
#include "vectors/stm32h7a3xxq_vectors.h"
#elif defined(STM32H7B3xxQ)
#include "vectors/stm32h7b3xxq_vectors.h"
#elif defined(STM32H735xx)
#include "vectors/stm32h735xx_vectors.h"
#elif defined(STM32H733xx)
#include "vectors/stm32h733xx_vectors.h"
#elif defined(STM32H730xx)
#include "vectors/stm32h730xx_vectors.h"
#elif defined(STM32H730xxQ)
#include "vectors/stm32h730xxq_vectors.h"
#elif defined(STM32H725xx)
#include "vectors/stm32h725xx_vectors.h"
#elif defined(STM32H723xx)
#include "vectors/stm32h723xx_vectors.h"
#else
#error "Please select first the target STM32H7xx device used in your application (in stm32h7xx_vectors.h file)"
#endif
