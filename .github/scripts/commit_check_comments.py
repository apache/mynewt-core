#!/usr/bin/env python3
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

"""
Posts commit message problems found by commit-check action as single pull
request comment, run by commit-check-comments action. The comment is
updated on following runs and deleted once all commits are fine. Pull
request code is never checked out or executed.

Results directory (uploaded by commit-check action) is untrusted input,
as it comes from a pull request run:
- pr-number - number of pull request that was checked
- problems.json - check_commits.py --json output

Results are used only if pull request head is still HEAD_SHA (head of
the checked run), otherwise they are stale or forged.
"""

import json
import os
import re
import subprocess
import sys

from mynewt_format_comments import REPO, HEAD_SHA, gh, gh_list, text

MARKER = "<!-- commit-check -->"
MAX_COMMITS = 50
MAX_PROBLEMS = 10

SHA_RE = re.compile(r"^[0-9a-f]{40}$")
GUIDE_URL = ("https://cwiki.apache.org/confluence/display/MYNEWT/"
             "Contributing+to+Apache+Mynewt")


def read_results(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            results = json.load(f)
    except ValueError:
        return []
    if not isinstance(results, list):
        return []
    return [r for r in results[:MAX_COMMITS]
            if isinstance(r, dict) and isinstance(r.get("sha"), str) and
            SHA_RE.match(r["sha"]) and isinstance(r.get("problems"), list)]


def comment_body(results):
    lines = [
        f"**Commit message check** found problems in {len(results)} "
        "commit(s):",
        "",
    ]
    for r in results:
        subject = text(r.get("subject"), 100).replace("`", "'")
        lines.append(f"- {r['sha']} `{subject}`")
        for p in r["problems"][:MAX_PROBLEMS]:
            lines.append(f"  - {text(p, 300)}")
    lines += [
        "",
        "Messages are `subsystem: Description` subject (at most 72 "
        "characters), empty line, body explaining why the change is made "
        "(lines of at most 72 characters) and optional tags "
        "(e.g. `Signed-off-by: Name <email>`) as the last paragraph. "
        "Subsystems that are not paths are listed in "
        "`.github/commit-subsystems`.",
        "",
        "Check locally (script is in apache-mynewt-core): "
        "`.github/scripts/check_commits.py origin/master`. See also "
        f"[contributing guidelines]({GUIDE_URL}).",
        MARKER,
    ]
    return "\n".join(lines)


def main():
    if len(sys.argv) != 2:
        sys.exit(f"usage: {sys.argv[0]} RESULTS_DIR")
    results_dir = sys.argv[1]

    with open(os.path.join(results_dir, "pr-number")) as f:
        pr_text = f.read(64).strip()
    if not pr_text.isdigit():
        sys.exit(f"Bad PR number '{pr_text[:20]}'")
    pr = int(pr_text)

    head = gh("api", f"repos/{REPO}/pulls/{pr}", "--jq", ".head.sha").strip()
    if head != HEAD_SHA:
        print(f"PR #{pr} head moved on, nothing to do")
        return

    path = os.path.join(results_dir, "problems.json")
    if not os.path.exists(path):
        print("Check did not finish, nothing to do")
        return
    results = read_results(path)
    old =[c for c in gh_list(f"repos/{REPO}/issues/{pr}/comments")
           if MARKER in (c.get("body") or "") and
           (c.get("user") or {}).get("type") == "Bot"]

    try:
        if not results:
            for c in old:
                gh("api", "--method", "DELETE",
                   f"repos/{REPO}/issues/comments/{c['id']}")
            print("All commit messages are fine")
            return

        body = json.dumps({"body": comment_body(results)})
        if old:
            gh("api", "--method", "PATCH",
               f"repos/{REPO}/issues/comments/{old[-1]['id']}",
               "--input", "-", input=body)
        else:
            gh("api", "--method", "POST",
               f"repos/{REPO}/issues/{pr}/comments",
               "--input", "-", input=body)
    except subprocess.CalledProcessError as e:
        sys.exit(f"Failed to update comment: {e.stderr.strip()}")
    print(f"Commented on {len(results)} commit(s)")


if __name__ == "__main__":
    main()
