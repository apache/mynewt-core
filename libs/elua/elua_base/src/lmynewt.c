/**
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

#include <assert.h>
#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "shell/shell.h"
#include "elua_base/elua.h"

#ifdef MYNEWT

#if MYNEWT_VAL(ELUA_CLI)
static int lua_cmd(int argc, char **argv);

static struct shell_cmd lua_shell_cmd = {
    .sc_cmd = "lua",
    .sc_cmd_func = lua_cmd
};

static int
lua_cmd(int argc, char **argv)
{
    lua_main(argc, argv);
    return 0;
}
#endif

void
lua_init(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(ELUA_CLI)
    rc = shell_cmd_register(&lua_shell_cmd);
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}
#endif
