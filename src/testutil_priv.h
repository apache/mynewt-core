#ifndef H_TESTUTIL_PRIV_
#define H_TESTUTIL_PRIV_

#include <stddef.h>
#include <inttypes.h>

int tu_report_mkdir_results(void);
int tu_report_mkdir_suite(void);
int tu_report_mkdir_case(void);
int tu_report_write_file(const char *filename, const uint8_t *data,
                         size_t data_len);

int tu_io_write(const char *path, const uint8_t *contents, size_t len);

int tu_io_mkdir(const char *path);

#endif
