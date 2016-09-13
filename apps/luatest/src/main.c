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
#include <string.h>
#include "os/os.h"
#include "bsp/bsp.h"
#include "elua_base/elua.h"
#include "fs/fs.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

static void
create_script_file(void)
{
    char filename[] = "/foobar";
    char script[] = "print \"eat my shorts\"\n";
    struct fs_file *nf;
    int rc;

    rc = fs_open(filename, FS_ACCESS_READ, &nf);
    if (rc) {
        rc = fs_open(filename, FS_ACCESS_WRITE, &nf);
        assert(rc == 0);
        rc = fs_write(nf, script, strlen(script));
        assert(rc == 0);
    }
    fs_close(nf);
}

/**
 * main
 *
 * The main function for the project. This function initializes and starts the
 * OS.  We should not return from os start.
 *
 * @return int NOTE: this function should never return!
 */
int
main(int argc, char **argv)
{
#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    /* Initialize OS */
    os_init();

    create_script_file();

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return 0;
}
