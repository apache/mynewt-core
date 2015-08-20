#ifndef H_FFSUTIL_
#define H_FFSUTIL_

#include <inttypes.h>

int ffsutil_read_file(const char *path, uint32_t offset, uint32_t len,
                      void *dst, uint32_t *out_len);
int ffsutil_write_file(const char *path, const void *data, uint32_t len);

#endif
