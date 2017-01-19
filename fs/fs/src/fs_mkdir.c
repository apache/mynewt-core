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
#include <fs/fs.h>
#include <fs/fs_if.h>

#include "fs_priv.h"

struct fs_ops *fops_from_filename(const char *);

int
fs_rename(const char *from, const char *to)
{
    struct fs_ops *fops = fops_from_filename(from);
    return fops->f_rename(from, to);
}

int
fs_mkdir(const char *path)
{
    struct fs_ops *fops = fops_from_filename(path);
    return fops->f_mkdir(path);
}
