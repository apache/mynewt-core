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

#ifndef H_TESTREPORT_PRIV_
#define H_TESTREPORT_PRIV_

#include <stddef.h>
#include <inttypes.h>

int tr_results_mkdir_results(void);
int tr_results_rmdir_results(void);

int tr_results_mkdir_meta(void);

int tr_results_mkdir_suite(void);
int tr_results_mkdir_case(void);

int tr_results_write_file(const char *filename, const uint8_t *data,
                          size_t data_len);

int tr_results_read_status(void);
int tr_results_write_status(void);

int tr_io_read(const char *path, void *out_data, size_t len, size_t *out_len);
int tr_io_write(const char *path, const void *contents, size_t len);

int tr_io_delete(const char *path);

int tr_io_mkdir(const char *path);
int tr_io_rmdir(const char *path);

#endif
