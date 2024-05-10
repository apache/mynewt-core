#!/usr/bin/python3

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
import re
import subprocess
import sys
import shutil
from pathlib import Path

tmp_folder = "/tmp/check_license/"
licenses_url = "https://www.apache.org/legal/resolved.html"
LICENSE_FILE = "LICENSE"
RAT_EXCLUDES_FILE = ".rat-excludes"
ignored = {
    ".gdb_history",
    ".gdb_out",
    "tags",
    "bin"
}


def run_cmd(cmd: str) -> list[str]:
    out = subprocess.check_output(cmd, text=True, shell=True)
    return out.splitlines()


def run_cmd_no_check(cmd: str) -> list[str]:
    out = subprocess.run(cmd, text=True, shell=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL).stdout
    return out.splitlines()


def get_rat_exclusions(file_path: str) -> set[str]:
    file_names = set()
    try:
        with open(file_path, 'r') as file:
            lines = file.readlines()
            for line in lines:
                line = re.sub(r'#.*', '', line)
                line = line.strip()
                if line:
                    file_names.add(line)
    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found.")
    except IOError as e:
        print(f"Error reading file '{file_path}': {e}")
    return file_names


def get_license_files(file_path: str) -> set[str]:
    file_paths = set()
    try:
        with open(file_path, 'r') as file:
            lines = file.readlines()
            for line in lines:
                line = line.strip()
                if line.startswith('*'):
                    line = line[1:].strip()
                    if line:
                        file_paths.add(line)
    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found.")
    except IOError as e:
        print(f"Error reading file '{file_path}': {e}")
    return file_paths


def check_rat_exclusions(rat_entries: set[str], ignored_entries: set[str]) -> list[str]:
    result = []
    base_dir = Path.cwd()
    existing_paths = {path.name for path in base_dir.rglob('*')}
    for entry in rat_entries:
        if entry not in existing_paths and entry not in ignored_entries:
            result.append(entry)
    return result


def check_license_files(license_entries: set[str]) -> list[str]:
    result = []
    base_dir = Path.cwd()
    for entry in license_entries:
        full_path = base_dir / entry
        if not full_path.exists():
            result.append(entry)
    return result


def run_rat_check(files_diff: list[str]) -> list[str]:
    result = []
    for cfg_fname in files_diff:
        os.makedirs(str(Path(tmp_folder + cfg_fname).parent), 0o755, True)
        shutil.copy(cfg_fname, tmp_folder + cfg_fname)
    rat_out = run_cmd_no_check(f"java -jar apache-rat.jar -E .rat-excludes -d {tmp_folder} | grep \"^ !\"")
    if rat_out:
        for entry in rat_out:
            result.append(entry.strip(' !').replace(tmp_folder, '\t\t'))
    return result


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
    result_rat_check = run_rat_check(files)

    files = get_license_files(LICENSE_FILE)
    result_license = check_license_files(files)

    files = get_rat_exclusions(RAT_EXCLUDES_FILE)
    result_rat_exclusions = check_rat_exclusions(files, ignored)

    if any([result_license, result_rat_exclusions, result_rat_check]):
        if result_rat_check:
            print(f"\033[31m! Files with unapproved or unknown licenses detected.\033[0m")
            print(f"\033[31m! See {licenses_url} for details.\033[0m")
            print()
            print(f"\033[90mLicense\t\tFilename\033[0m")
            for result in result_rat_check:
                print(result)
            print()
        if result_license:
            print(f"\033[31m! 'LICENSE' file contains paths to files or "
                  f"directories that do not exist in the repository:\033[0m")
            for result in result_license:
                print(result)
            print()
        if result_rat_exclusions:
            print(f"\033[31m! File '.rat-excludes' contains files or directories"
                  f" names that do not exist in the repository:\033[0m")
            for result in result_rat_exclusions:
                print(result)
            print()
        return False

    return True


if __name__ == "__main__":
    if not main():
        sys.exit(1)
