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

import urllib.request
import urllib.error
import json
import re

URL = "https://static.codecoup.pl/mynewt/run/latest/status.json"
BASE_URL = "https://static.codecoup.pl/mynewt/run/latest"
README_PATH = "README.md"
START_MARKER = "<!--HW_CI_START-->"
END_MARKER = "<!--HW_CI_END-->"


def _get_results_from_url(url):
    try:
        with urllib.request.urlopen(url) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        print(f"Connection error: {e}")


def _generate_markdown(results):
    md = [f"## HW CI status"]

    for test_name in sorted(results.keys()):
        md.append(f"![]({BASE_URL}/{test_name}.svg)")

    md.append("\n<details>")
    md.append("<summary>Toggle for more informations</summary>\n")

    for test_name in sorted(results.keys()):
        boards = results[test_name]
        valid_boards = sorted([b for b, status in boards.items() if status in ("passing", "failing")])

        if valid_boards:
            md.append(f"## {test_name}")
            for board in valid_boards:
                md.append(f"![]({BASE_URL}/{test_name}/{board}.svg)")
            md.append("")

    md.append("</details>")

    return "\n".join(md)


def _update_readme(new_md_block):
    try:
        with open(README_PATH, "r", encoding="utf-8") as f:
            content = f.read()

        pattern = re.compile(rf"{START_MARKER}(?P<HW_CI>.*?){END_MARKER}", re.DOTALL)

        m = pattern.search(content)

        if not m:
            print(f"Markers {START_MARKER} and {END_MARKER} not found in README.md")
            return

        old_md_block = m.group("HW_CI").strip()

        if old_md_block == new_md_block:
            print(f"No differences in HW CI status, skipping update.")
        else:
            new_content = pattern.sub(rf"{START_MARKER}\n{new_md_block}\n{END_MARKER}", content)
            with open(README_PATH, "w", encoding="utf-8") as f:
                f.write(new_content)
            print("README.md content updated successfully")

    except Exception as e:
        print(f"Error occured while updating README: {e}")


def main():
    results = _get_results_from_url(URL)

    if not results:
        print("Data could not be requested or JSON is empty")
        return

    new_md_block = _generate_markdown(results)
    _update_readme(new_md_block)


if __name__ == "__main__":
    main()
