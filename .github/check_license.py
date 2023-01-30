#!/usr/bin/python

#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

import os
import subprocess
import sys
import shutil
from pathlib import Path

tmp_folder = "/tmp/check_license/"
licenses_url = "https://www.apache.org/legal/resolved.html"

def run_cmd(cmd: str) -> list[str]:
    out = subprocess.check_output(cmd, text=True, shell=True)
    return out.splitlines()


def run_cmd_no_check(cmd: str) -> list[str]:
    out = subprocess.run(cmd, text=True, shell=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL).stdout
    return out.splitlines()


def main() -> bool:
    if len(sys.argv) > 1:
        commit = sys.argv[1]
    else:
        commit = "HEAD"
    if len(sys.argv) > 2:
        upstream = sys.argv[2]
    else:
        upstream = "origin/master"

    mb = run_cmd(f"git merge-base {upstream} {commit}")
    upstream = mb[0]

    files = run_cmd(f"git diff --diff-filter=AM --name-only {upstream} {commit}")
    for cfg_fname in files:
        os.makedirs(str(Path(tmp_folder + cfg_fname).parent), 0o755, True)
        shutil.copy(cfg_fname, tmp_folder + cfg_fname)

    files = run_cmd_no_check(f"java -jar apache-rat.jar -E .rat-excludes -d {tmp_folder} | grep \"^ !\"")
    if files :
        print(f"\033[31m! Files with unapproved or unknown licenses detected.\033[0m")
        print(f"\033[31m! See {licenses_url} for details.\033[0m")
        print()
        print(f"\033[90mLicense\t\tFilename\033[0m")
        for f in files:
            print(f.strip(' !').replace(tmp_folder,'\t\t'))
        return False

    return True


if __name__ == "__main__":
    if not main():
        sys.exit(1)
