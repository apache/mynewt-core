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

#if defined(STM32WB55xx)
#include "vectors/stm32wb55xx_cm4_vectors.h"
#elif defined(STM32WB5Mxx)
#include "vectors/stm32wb5mxx_cm4_vectors.h"
#elif defined(STM32WB50xx)
#include "vectors/stm32wb50xx_cm4_vectors.h"
#elif defined(STM32WB35xx)
#include "vectors/stm32wb35xx_cm4_vectors.h"
#elif defined(STM32WB30xx)
#include "vectors/stm32wb30xx_cm4_vectors.h"
#elif defined(STM32WB15xx)
#include "vectors/stm32wb15xx_cm4_vectors.h"
#elif defined(STM32WB10xx)
#include "vectors/stm32wb10xx_cm4_vectors.h"
#elif defined(STM32WB1Mxx)
#include "vectors/stm32wb1mxx_cm4_vectors.h"
#else
#error "Please select first the target STM32WBxx device used in your application, for instance xxx (in stm32wbxx.h file)"
#endif
