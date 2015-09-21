/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef H_HAL_FLASH_
#define H_HAL_FLASH_

#include <inttypes.h>

int flash_read(uint32_t address, void *dst, uint32_t num_bytes);
int flash_write(uint32_t address, const void *src, uint32_t num_bytes);
int flash_erase_sector(uint32_t sector_address);
int flash_erase(uint32_t address, uint32_t num_bytes);
int flash_init(void);

#endif

