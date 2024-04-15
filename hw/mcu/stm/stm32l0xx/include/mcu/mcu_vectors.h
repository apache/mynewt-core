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

#if defined(STM32L010xB)
#include "vectors/stm32l010xb_vectors.h"
#elif defined(STM32L010x8)
#include "vectors/stm32l010x8_vectors.h"
#elif defined(STM32L010x6)
#include "vectors/stm32l010x6_vectors.h"
#elif defined(STM32L010x4)
#include "vectors/stm32l010x4_vectors.h"
#elif defined(STM32L011xx)
#include "vectors/stm32l011xx_vectors.h"
#elif defined(STM32L021xx)
#include "vectors/stm32l021xx_vectors.h"
#elif defined(STM32L031xx)
#include "vectors/stm32l031xx_vectors.h"
#elif defined(STM32L041xx)
#include "vectors/stm32l041xx_vectors.h"
#elif defined(STM32L051xx)
#include "vectors/stm32l051xx_vectors.h"
#elif defined(STM32L052xx)
#include "vectors/stm32l052xx_vectors.h"
#elif defined(STM32L053xx)
#include "vectors/stm32l053xx_vectors.h"
#elif defined(STM32L062xx)
#include "vectors/stm32l062xx_vectors.h"
#elif defined(STM32L063xx)
#include "vectors/stm32l063xx_vectors.h"
#elif defined(STM32L071xx)
#include "vectors/stm32l071xx_vectors.h"
#elif defined(STM32L072xx)
#include "vectors/stm32l072xx_vectors.h"
#elif defined(STM32L073xx)
#include "vectors/stm32l073xx_vectors.h"
#elif defined(STM32L082xx)
#include "vectors/stm32l082xx_vectors.h"
#elif defined(STM32L083xx)
#include "vectors/stm32l083xx_vectors.h"
#elif defined(STM32L081xx)
#include "vectors/stm32l081xx_vectors.h"
#else
#error "Please select first the target STM32L0xx device used in your application"
#endif
