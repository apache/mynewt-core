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

#if defined(STM32H573xx)
#include "vectors/stm32h573xx_vectors.h"
#elif defined(STM32H563xx)
#include "vectors/stm32h563xx_vectors.h"
#elif defined(STM32H562xx)
#include "vectors/stm32h562xx_vectors.h"
#elif defined(STM32H523xx)
#include "vectors/stm32h523xx_vectors.h"
#elif defined(STM32H533xx)
#include "vectors/stm32h533xx_vectors.h"
#elif defined(STM32H503xx)
#include "vectors/stm32h503xx_vectors.h"
#else
#error "Please select first the target STM32H5xx device used in your application (in stm32h5xx.h file)"
#endif
