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

#if defined(STM32G0B1xx)
#include "vectors/stm32g0b1xx_vectors.h"
#elif defined(STM32G0C1xx)
#include "vectors/stm32g0c1xx_vectors.h"
#elif defined(STM32G0B0xx)
#include "vectors/stm32g0b0xx_vectors.h"
#elif defined(STM32G071xx)
#include "vectors/stm32g071xx_vectors.h"
#elif defined(STM32G081xx)
#include "vectors/stm32g081xx_vectors.h"
#elif defined(STM32G070xx)
#include "vectors/stm32g070xx_vectors.h"
#elif defined(STM32G031xx)
#include "vectors/stm32g031xx_vectors.h"
#elif defined(STM32G041xx)
#include "vectors/stm32g041xx_vectors.h"
#elif defined(STM32G030xx)
#include "vectors/stm32g030xx_vectors.h"
#elif defined(STM32G051xx)
#include "vectors/stm32g051xx_vectors.h"
#elif defined(STM32G061xx)
#include "vectors/stm32g061xx_vectors.h"
#elif defined(STM32G050xx)
#include "vectors/stm32g050xx_vectors.h"
#else
#error "Please select first the target STM32G0xx device used in your application"
#endif
