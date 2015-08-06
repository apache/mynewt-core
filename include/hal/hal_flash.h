#ifndef H_HAL_FLASH_
#define H_HAL_FLASH_

#include <inttypes.h>

int flash_read(uint32_t address, void *dst, uint32_t num_bytes);
int flash_write(uint32_t address, const void *src, uint32_t num_bytes);
int flash_erase_sector(uint32_t sector_address);
int flash_erase(uint32_t address, uint32_t num_bytes);
int flash_init(void);

#endif

