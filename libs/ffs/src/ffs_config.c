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

#include "ffs/ffs.h"

struct ffs_config ffs_config;

const struct ffs_config ffs_config_dflt = {
    .fc_num_inodes = 100,
    .fc_num_blocks = 100,
    .fc_num_files = 4,
    .fc_num_cache_inodes = 4,
    .fc_num_cache_blocks = 64,
};

void
ffs_config_init(void)
{
    if (ffs_config.fc_num_inodes == 0) {
        ffs_config.fc_num_inodes = ffs_config_dflt.fc_num_inodes;
    }
    if (ffs_config.fc_num_blocks == 0) {
        ffs_config.fc_num_blocks = ffs_config_dflt.fc_num_blocks;
    }
    if (ffs_config.fc_num_files == 0) {
        ffs_config.fc_num_files = ffs_config_dflt.fc_num_files;
    }
    if (ffs_config.fc_num_cache_inodes == 0) {
        ffs_config.fc_num_cache_inodes = ffs_config_dflt.fc_num_cache_inodes;
    }
    if (ffs_config.fc_num_cache_blocks == 0) {
        ffs_config.fc_num_cache_blocks = ffs_config_dflt.fc_num_cache_blocks;
    }
}
