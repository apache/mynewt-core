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

#if defined(STM32F405xx)
#include "vectors/stm32f405xx_vectors.h"
#elif defined(STM32F415xx)
#include "vectors/stm32f415xx_vectors.h"
#elif defined(STM32F407xx)
#include "vectors/stm32f407xx_vectors.h"
#elif defined(STM32F417xx)
#include "vectors/stm32f417xx_vectors.h"
#elif defined(STM32F427xx)
#include "vectors/stm32f427xx_vectors.h"
#elif defined(STM32F437xx)
#include "vectors/stm32f437xx_vectors.h"
#elif defined(STM32F429xx)
#include "vectors/stm32f429xx_vectors.h"
#elif defined(STM32F439xx)
#include "vectors/stm32f439xx_vectors.h"
#elif defined(STM32F401xC)
#include "vectors/stm32f401xc_vectors.h"
#elif defined(STM32F401xE)
#include "vectors/stm32f401xe_vectors.h"
#elif defined(STM32F410Tx)
#include "vectors/stm32f410tx_vectors.h"
#elif defined(STM32F410Cx)
#include "vectors/stm32f410cx_vectors.h"
#elif defined(STM32F410Rx)
#include "vectors/stm32f410rx_vectors.h"
#elif defined(STM32F411xE)
#include "vectors/stm32f411xe_vectors.h"
#elif defined(STM32F446xx)
#include "vectors/stm32f446xx_vectors.h"
#elif defined(STM32F469xx)
#include "vectors/stm32f469xx_vectors.h"
#elif defined(STM32F479xx)
#include "vectors/stm32f479xx_vectors.h"
#elif defined(STM32F412Cx)
#include "vectors/stm32f412cx_vectors.h"
#elif defined(STM32F412Zx)
#include "vectors/stm32f412zx_vectors.h"
#elif defined(STM32F412Rx)
#include "vectors/stm32f412rx_vectors.h"
#elif defined(STM32F412Vx)
#include "vectors/stm32f412vx_vectors.h"
#elif defined(STM32F413xx)
#include "vectors/stm32f413xx_vectors.h"
#elif defined(STM32F423xx)
#include "vectors/stm32f423xx_vectors.h"
#else
#error "Please select first the target STM32F4xx device used in your application"
#endif
