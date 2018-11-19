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

#ifndef HW_BUS_DEBUG_H_
#define HW_BUS_DEBUG_H_

#include "syscfg/syscfg.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MYNEWT_VAL(BUS_DEBUG_OS_DEV)
#define BUS_DEBUG_MAGIC_DEV             0xABADBABE
#define BUS_DEBUG_MAGIC_NODE            0xABADCAFE
#define BUS_DEBUG_POISON_DEV(_dev)      do { (_dev)->devmagic = (BUS_DEBUG_MAGIC_DEV); } while (0)
#define BUS_DEBUG_POISON_NODE(_node)    do { (_node)->nodemagic = (BUS_DEBUG_MAGIC_NODE); } while (0)
#define BUS_DEBUG_VERIFY_DEV(_dev)      assert((_dev)->devmagic == (BUS_DEBUG_MAGIC_DEV))
#define BUS_DEBUG_VERIFY_NODE(_node)    assert((_node)->nodemagic == (BUS_DEBUG_MAGIC_NODE))
#else
#define BUS_DEBUG_POISON_DEV(_dev)      (void)(_dev)
#define BUS_DEBUG_POISON_NODE(_node)    (void)(_node)
#define BUS_DEBUG_VERIFY_DEV(_dev)      (void)(_dev)
#define BUS_DEBUG_VERIFY_NODE(_node)    (void)(_node)
#endif

#ifdef __cplusplus
}
#endif

#endif /* HW_BUS_DEBUG_H_ */
