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
Checks messages of commits in BASE..HEAD against Mynewt commit message
style:

    subsystem: Short description of the change

    Body explaining why the change is made, lines of at most 72
    characters.

    Signed-off-by: Name <email>
    Assisted-by: Tool (model)

Subsystem names part of the code the commit changes. Each of its '/'
separated parts must be a directory or file name of a path changed by the
commit, e.g. 'hw/mcu/nordic' for hw/mcu/nordic/nrf52xxx/src/hal_gpio.c.
Names that are not paths (e.g. 'ci' for .github) are listed in subsystems
file:

    # subsystem    path(s) it stands for
    ci             .github
    nimble/ll      nimble/controller

Tags are optional. If present, they are the last paragraph of the message
and are not part of the body, so the 72 characters limit does not apply.

Run locally: .github/scripts/check_commits.py origin/master
"""

import argparse
import json
import os
import re
import subprocess
import sys

MAX_LEN = 72

SUBJECT_RE = re.compile(r"^(?P<subsys>[^\s:]+): (?P<msg>.*)$")
SUBSYS_RE = re.compile(r"^[A-Za-z0-9_.+-]+(/[A-Za-z0-9_.+-]+)*$")
TAG_RE = re.compile(r"^(?P<key>[A-Za-z][A-Za-z0-9-]*):(?P<value>.*)$")
PERSON_RE = re.compile(r"^[^<>@]+ <[^<>\s]+@[^<>\s]+>$")
UNSQUASHED_RE = re.compile(r"^(fixup!|squash!|amend!|\[?wip\b)",
                           re.IGNORECASE)

# Tag keys (lower case) and whether their value is 'Name <email>'
TAGS = {
    "signed-off-by": True,
    "co-authored-by": True,
    "reviewed-by": True,
    "acked-by": True,
    "tested-by": True,
    "reported-by": True,
    "suggested-by": True,
    "assisted-by": False,
    "generated-by": False,
}


def git(*args):
    return subprocess.run(["git", *args], check=True, capture_output=True,
                          text=True).stdout


def read_subsystems(path):
    """Returns dict of subsystem name to paths it stands for"""
    aliases = {}
    if not path or not os.path.exists(path):
        return aliases
    with open(path) as f:
        for line in f:
            words = line.split("#", 1)[0].split()
            if len(words) >= 2:
                aliases[words[0]] = words[1:]
    return aliases


def path_names(files):
    """Returns lower case directory and file names of paths, with words
    of names too, so that e.g. 'gap' matches ble_gap.c"""
    names = set()
    for f in files:
        for part in f.lower().split("/"):
            part = part.lstrip(".")
            names.add(part)
            names.add(os.path.splitext(part)[0])
            names.update(re.split(r"[_.-]", part))
    return names


def check_subsystem(subsys, files, aliases):
    if not SUBSYS_RE.match(subsys):
        return f"subsystem '{subsys}' is not a path like 'hw/mcu/nordic'"
    if not files:
        return None

    # Subsystem itself, or with its longest aliased part replaced
    candidates = [subsys]
    parts = subsys.split("/")
    for i in range(len(parts), 0, -1):
        alias = "/".join(parts[:i])
        if alias in aliases:
            rest = parts[i:]
            candidates += ["/".join([p, *rest]) for p in aliases[alias]]
            break

    names = path_names(files)
    for c in candidates:
        if all(part.lower().lstrip(".") in names for part in c.split("/")):
            return None

    return (f"subsystem '{subsys}' does not match changed files, add it "
            "to subsystems file if it is not a path")


def split_tags(paragraphs):
    """Returns (body paragraphs, tag lines). Tags are the last paragraph,
    if all its lines look like tags and at least one is a known tag."""
    if paragraphs:
        keys = [TAG_RE.match(line) for line in paragraphs[-1]]
        if all(keys) and any(k.group("key").lower() in TAGS for k in keys):
            return paragraphs[:-1], paragraphs[-1]
    return paragraphs, []


def check_tag(line):
    m = TAG_RE.match(line)
    key = m.group("key")
    if key.lower() not in TAGS:
        return f"unknown tag '{key}'"
    value = m.group("value")
    if value != " " + value.strip() or not value.strip():
        return f"tag '{key}' must be '{key}: value'"
    if TAGS[key.lower()] and not PERSON_RE.match(value.strip()):
        return f"tag '{key}' must be '{key}: Name <email>'"
    return None


def check_message(message, files, aliases):
    problems = []
    lines = message.rstrip("\n").split("\n")
    subject = lines[0]

    if UNSQUASHED_RE.match(subject):
        return ["not squashed (fixup!/squash!/WIP commit)"]
    if len(subject) > MAX_LEN:
        problems.append(f"subject longer than {MAX_LEN} characters "
                        f"({len(subject)})")
    if subject != subject.rstrip():
        problems.append("subject has trailing whitespace")

    m = SUBJECT_RE.match(subject.rstrip())
    if not m:
        problems.append("subject must be 'subsystem: Description', with "
                        "one space after the colon")
    else:
        msg = m.group("msg")
        if not msg or not msg[0].isupper() or msg != msg.lstrip():
            problems.append("description must start with a capital letter")
        if msg.endswith("."):
            problems.append("subject must not end with a period")
        p = check_subsystem(m.group("subsys"), files, aliases)
        if p:
            problems.append(p)

    if len(lines) > 1 and lines[1].strip():
        problems.append("subject must be followed by an empty line")

    # Paragraphs of body and tags
    paragraphs = []
    for line in lines[1:]:
        if not line.strip():
            if paragraphs and paragraphs[-1]:
                paragraphs.append([])
        elif paragraphs:
            paragraphs[-1].append(line)
        else:
            paragraphs.append([line])
    paragraphs = [p for p in paragraphs if p]

    body, tags = split_tags(paragraphs)
    if not body:
        problems.append("body is missing")

    for n, para in enumerate(body):
        for line in para:
            if len(line) > MAX_LEN:
                problems.append(f"body line longer than {MAX_LEN} "
                                f"characters ({len(line)}): '{line}'")
            if line != line.rstrip():
                problems.append(f"body line has trailing whitespace: "
                                f"'{line.rstrip()}'")
            m = TAG_RE.match(line)
            if m and m.group("key").lower() in TAGS:
                if n == len(body) - 1 and not tags:
                    problems.append("tags must be separated from body by "
                                    "an empty line")
                else:
                    problems.append(f"tag '{m.group('key')}' must be in "
                                    "the last paragraph")

    for line in tags:
        p = check_tag(line)
        if p:
            problems.append(p)

    return problems


def check_commits(base, head, aliases):
    """Returns list of dicts with sha, subject and problems of commits
    that break the style"""
    results = []
    revs = git("rev-list", "--reverse", "--parents", f"{base}..{head}")
    for line in revs.splitlines():
        sha, *parents = line.split()
        # Messages of bots (e.g. dependabot) can't be changed
        if git("log", "-1", "--format=%an", sha).strip().endswith("[bot]"):
            continue
        message = git("log", "-1", "--format=%B", sha)
        subject = message.split("\n", 1)[0]
        if len(parents) > 1:
            problems = ["merge commit, rebase on the target branch instead"]
        else:
            files = git("diff-tree", "--no-commit-id", "--name-only", "-r",
                        "--root", sha).split("\n")
            problems = check_message(message, [f for f in files if f],
                                     aliases)
        if problems:
            results.append({"sha": sha, "subject": subject,
                            "problems": problems})
    return results


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("base", help="e.g. origin/master")
    parser.add_argument("head", nargs="?", default="HEAD")
    parser.add_argument("--subsystems", default=".github/commit-subsystems",
                        help="subsystems file (default: %(default)s)")
    parser.add_argument("--json", metavar="FILE",
                        help="write problems to FILE as JSON")
    args = parser.parse_args()

    base = git("merge-base", args.base, args.head).strip()
    results = check_commits(base, args.head, read_subsystems(args.subsystems))

    if args.json:
        with open(args.json, "w") as f:
            json.dump(results, f)

    for r in results:
        print(f"{r['sha'][:12]} {r['subject']}")
        for p in r["problems"]:
            print(f"    {p}")
    if results:
        print(f"\n{len(results)} commit(s) do not follow commit message "
              "style, see check_commits.py --help")
        return 1
    print("All commit messages follow the style")
    return 0


if __name__ == "__main__":
    sys.exit(main())
