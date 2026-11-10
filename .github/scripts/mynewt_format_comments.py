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
Posts problems found by mynewt-format check as review comments on pull
request lines that break a rule, run by mynewt-format-comments action.
Pull request code is never checked out or executed, everything is done
through GitHub API using gh tool.

Results directory (uploaded by mynewt-format-check action) is untrusted
input, as it comes from a pull request run:
- pr-number - number of pull request that was checked
- problems.jsonl - mynewt-format --format json output

Results are used only if pull request head is still HEAD_SHA (head of
the checked run), otherwise they are stale or forged.

If there are more than MAX_COMMENTS new problems, no review comments are
posted. Instead, single pull request comment (updated on following runs)
recommends running mynewt-format locally.
"""

import json
import os
import re
import subprocess
import sys

MARKER = "<!-- mynewt-format"
TOO_MANY_MARKER = "<!-- mynewt-format: too-many -->"
MAX_COMMENTS = 50
MAX_ELSEWHERE = 100

REPO = os.environ.get("GITHUB_REPOSITORY", "")
HEAD_SHA = os.environ.get("HEAD_SHA", "")

RULE_RE = re.compile(r"^[a-z0-9-]+$")
HUNK_RE = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@")
POSTED_RE = re.compile(r"<!-- mynewt-format: (\S+) -->")


def gh(*args, input=None):
    """Runs gh command, returns its output"""
    return subprocess.run(["gh", *args], check=True, capture_output=True,
                          text=True, input=input).stdout


def gh_list(path):
    """Returns all items of paginated list API"""
    out = gh("api", "--paginate", f"{path}?per_page=100", "--jq", ".[]")
    return [json.loads(line) for line in out.splitlines() if line]


def text(s, max_len):
    """Untrusted text: limit its size and keep '@' from notifying anyone"""
    return str(s if s is not None else "")[:max_len].replace("@", "@​")


def read_problems(path):
    problems = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if not line.strip():
                continue
            try:
                p = json.loads(line)
            except ValueError:
                continue  # Not a problem record
            if (isinstance(p, dict) and isinstance(p.get("path"), str) and
                    type(p.get("line")) is int and
                    isinstance(p.get("rule"), str) and RULE_RE.match(p["rule"])):
                problems.append(p)
    return problems


def commentable_lines(files):
    """Returns dict of path to lines GitHub accepts comments on: new side
    of each diff hunk"""
    commentable = {}
    for f in files:
        lines = set()
        n = 0
        for line in (f.get("patch") or "").split("\n"):
            h = HUNK_RE.match(line)
            if h:
                n = int(h.group(1))
            elif line.startswith(("+", " ")):
                lines.add(n)
                n += 1
        commentable[f["filename"]] = lines
    return commentable


def posted_comments(comments):
    """Returns path:line:rule of comments posted by earlier runs, so new
    push does not repeat them"""
    posted = set()
    for c in comments:
        m = POSTED_RE.search(c.get("body") or "")
        if m and (c.get("user") or {}).get("type") == "Bot":
            posted.add(f"{c['path']}:{c['line']}:{m.group(1)}")
    return posted


def comment_body(group):
    p = group[0]
    kind = "warning" if p.get("severity") == "warning" else "error"
    messages = "; ".join(dict.fromkeys(text(g.get("message"), 300)
                                       for g in group))
    raw = str(p.get("source") if p.get("source") is not None else "")[:300]
    source = raw.expandtabs(4)
    carets = [" "] * (len(source) + 1)
    for g in group:
        col = g.get("column")
        col = col if type(col) is int and col > 0 else 1
        carets[min(len(raw[:col - 1].expandtabs(4)), len(carets) - 1)] = "^"
    return "\n".join([
        f"**mynewt-format** {kind} `{p['rule']}`: {messages}",
        "",
        "````c",
        text(source, 400),
        "".join(carets).rstrip(),
        "````",
        "",
        f"Details: `mynewt-format --explain {p['rule']}`",
        f"{MARKER}: {p['rule']} -->",
    ])


def elsewhere_line(group):
    p = group[0]
    kind = "warning" if p.get("severity") == "warning" else "error"
    messages = "; ".join(dict.fromkeys(text(g.get("message"), 300)
                                       for g in group))
    return (f"- `{text(p['path'], 200)}:{p['line']}` {kind} "
            f"`{p['rule']}`: {messages}")


def new_problems(problems, commentable, posted):
    """Returns problems of PR files not commented on yet, grouped by rule
    and line, as a rule can be broken more than once on a line (e.g. both
    sides of an operator)"""
    groups = {}
    for p in problems:
        key = f"{p['path']}:{p['line']}:{p['rule']}"
        if key in posted or p["path"] not in commentable:
            continue
        groups.setdefault(key, []).append(p)
    return list(groups.values())


def build_review(groups, commentable):
    """Returns (review comments, lines of problems not on diff lines)"""
    comments = []
    elsewhere = []
    for group in groups:
        p = group[0]
        if p["line"] not in commentable[p["path"]]:
            elsewhere.append(elsewhere_line(group))
            continue
        comments.append({
            "path": p["path"],
            "line": p["line"],
            "side": "RIGHT",
            "body": comment_body(group),
        })
    return comments, elsewhere


def too_many_body(count):
    return "\n".join([
        f"**mynewt-format** found {count} new coding style problems in the "
        "lines changed by this PR, too many to comment on each of them. "
        "Errors fail the coding style check, warnings do not.",
        "",
        "Please run mynewt-format locally and review the proposed fixes:",
        "",
        "```",
        "mynewt-format -i --git-diff origin/master",
        "```",
        "",
        "`tools/mynewt-format-setup.sh` in apache-mynewt-core installs the "
        "command. All problems are also listed in the log of the "
        "mynewt-format check.",
        TOO_MANY_MARKER,
    ])


def post_too_many(pr, count):
    """Posts comment recommending local run, or updates one posted before,
    so that each push doesn't add another one"""
    old = [c for c in gh_list(f"repos/{REPO}/issues/{pr}/comments")
           if TOO_MANY_MARKER in (c.get("body") or "") and
           (c.get("user") or {}).get("type") == "Bot"]
    body = json.dumps({"body": too_many_body(count)})
    if old:
        gh("api", "--method", "PATCH",
           f"repos/{REPO}/issues/comments/{old[-1]['id']}",
           "--input", "-", input=body)
    else:
        gh("api", "--method", "POST", f"repos/{REPO}/issues/{pr}/comments",
           "--input", "-", input=body)


def main():
    if len(sys.argv) != 2:
        sys.exit(f"usage: {sys.argv[0]} RESULTS_DIR")
    results = sys.argv[1]

    with open(os.path.join(results, "pr-number")) as f:
        pr_text = f.read(64).strip()
    if not pr_text.isdigit():
        sys.exit(f"Bad PR number '{pr_text[:20]}'")
    pr = int(pr_text)

    head = gh("api", f"repos/{REPO}/pulls/{pr}", "--jq", ".head.sha").strip()
    if head != HEAD_SHA:
        print(f"PR #{pr} head moved on, nothing to do")
        return

    problems = read_problems(os.path.join(results, "problems.jsonl"))
    if not problems:
        print("No mynewt-format problems")
        return

    commentable = commentable_lines(gh_list(f"repos/{REPO}/pulls/{pr}/files"))
    posted = posted_comments(gh_list(f"repos/{REPO}/pulls/{pr}/comments"))
    groups = new_problems(problems, commentable, posted)
    if not groups:
        print("All problems were already commented on")
        return

    if len(groups) > MAX_COMMENTS:
        try:
            post_too_many(pr, len(groups))
        except subprocess.CalledProcessError as e:
            sys.exit(f"Failed to post comment: {e.stderr.strip()}")
        print(f"{len(groups)} problems, recommended running mynewt-format "
              "locally")
        return

    comments, elsewhere = build_review(groups, commentable)

    body = [
        f"**mynewt-format** found {len(comments) + len(elsewhere)} new "
        "coding style problem(s) in the lines changed by this PR. Errors "
        "fail the coding style check, warnings do not.",
        "",
        "Check locally: `mynewt-format --git-diff origin/master` "
        "(`tools/mynewt-format-setup.sh` in apache-mynewt-core installs "
        "the command).",
    ]
    if elsewhere:
        body += ["", "Problems that could not be attached to a diff line:",
                 "", *elsewhere[:MAX_ELSEWHERE]]

    review = {
        "commit_id": head,
        "event": "COMMENT",
        "body": "\n".join(body),
        "comments": comments,
    }
    try:
        gh("api", "--method", "POST", f"repos/{REPO}/pulls/{pr}/reviews",
           "--input", "-", input=json.dumps(review))
    except subprocess.CalledProcessError as e:
        sys.exit(f"Failed to post review: {e.stderr.strip()}")
    print(f"Posted {len(comments)} comment(s)")


if __name__ == "__main__":
    main()
