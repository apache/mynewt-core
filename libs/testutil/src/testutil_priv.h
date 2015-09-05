#ifndef H_TESTUTIL_PRIV_
#define H_TESTUTIL_PRIV_

#include <stddef.h>
#include <inttypes.h>

int tu_report_mkdir_results(void);
int tu_report_rmdir_results(void);

int tu_report_mkdir_meta(void);

int tu_report_mkdir_suite(void);
int tu_report_mkdir_case(void);

int tu_report_write_file(const char *filename, const uint8_t *data,
                         size_t data_len);

int tu_report_read_status(void);
int tu_report_write_status(void);

int tu_io_read(const char *path, void *out_data, size_t len, size_t *out_len);
int tu_io_write(const char *path, const void *contents, size_t len);

int tu_io_delete(const char *path);

int tu_io_mkdir(const char *path);
int tu_io_rmdir(const char *path);

void tu_arch_restart(void);

void tu_case_abort(void);

#endif
