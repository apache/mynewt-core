#include "ffs/ffs.h"

struct ffs_config ffs_config;

const struct ffs_config ffs_config_dflt = {
    .fc_num_inodes = 100,
    .fc_num_blocks = 100,
    .fc_num_files = 16,
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
