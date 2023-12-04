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

#if defined(STM32G431xx)
#include "vectors/stm32g431xx_vectors.h"
#elif defined(STM32G441xx)
#include "vectors/stm32g441xx_vectors.h"
#elif defined(STM32G471xx)
#include "vectors/stm32g471xx_vectors.h"
#elif defined(STM32G473xx)
#include "vectors/stm32g473xx_vectors.h"
#elif defined(STM32G483xx)
#include "vectors/stm32g483xx_vectors.h"
#elif defined(STM32G474xx)
#include "vectors/stm32g474xx_vectors.h"
#elif defined(STM32G484xx)
#include "vectors/stm32g484xx_vectors.h"
#elif defined(STM32G491xx)
#include "vectors/stm32g491xx_vectors.h"
#elif defined(STM32G4A1xx)
#include "vectors/stm32g4a1xx_vectors.h"
#elif defined(STM32GBK1CB)
#include "vectors/stm32gbk1cb_vectors.h"
#else
#error "Please select first the target STM32G4xx device used in your application (in stm32g4xx.h file)"
#endif
