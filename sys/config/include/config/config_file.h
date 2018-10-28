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
#ifndef __SYS_CONFIG_FILE_H_
#define __SYS_CONFIG_FILE_H_

#include "config/config.h"
#include "config/config_store.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONF_FILE_NAME_MAX      32      /* max length for config filename */

struct fs_file;
struct conf_file {
    struct conf_store cf_store;
    const char *cf_name;                /* filename */
    int cf_maxlines;                    /* max # of lines before compressing */
    int cf_lines;                       /* private */
};

int conf_file_src(struct conf_file *);  /* register file to be source of cfg */
int conf_file_dst(struct conf_file *);  /* cfg saves go to a file */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_CONFIG_FILE_H_ */
