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

#include <stdlib.h>
#include "os/mynewt.h"

extern int main(int argc, char **argv);

/**
 * Rudimentary startup function.  Only called in the "application" half of a
 * split image.
 *
 * If the loader and app both used the same start symbol (_start), the
 * loader's main() gets used by the app, rather than the app's main.  This
 * causes the linker to strip pretty much all functionality from the app.
 *
 * The solution is to use a different start symbol for the "application"
 * half of a split image.  Rather than _start, the app uses _start_split.
 *
 * In addition, due to the way newt builds split images, _start_split has
 * to reside in a package that the loader doesn't use.  If it is in a
 * shared package, the entire package gets put in the loader, and
 * _start_split would erroneously reference the loader's main().
 */
void
_start_split(void)
{
#if !MYNEWT_VAL(OS_SCHEDULING)
    int rc;

    rc = main(0, NULL);
    exit(rc);
#else
    os_init(main);
    os_start();
#endif
}
