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

#if defined(STM32U575xx)
#include "vectors/stm32u575xx_vectors.h"
#elif defined(STM32U585xx)
#include "vectors/stm32u585xx_vectors.h"
#elif defined(STM32U595xx)
#include "vectors/stm32u595xx_vectors.h"
#elif defined(STM32U599xx)
#include "vectors/stm32u599xx_vectors.h"
#elif defined(STM32U5A5xx)
#include "vectors/stm32u5a5xx_vectors.h"
#elif defined(STM32U5A9xx)
#include "vectors/stm32u5a9xx_vectors.h"
#elif defined(STM32U535xx)
#include "vectors/stm32u535xx_vectors.h"
#elif defined(STM32U545xx)
#include "vectors/stm32u545xx_vectors.h"
#else
#error "Please select first the target STM32U5xx device used in your application (in stm32u5xx.h file)"
#endif
