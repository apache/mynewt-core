<!--
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
-->

# Apache Mynewt Core Tools

- **`mynewt-format.py`**: coding style checker and fixer. Configured by
  [`.mynewt-format`](../.mynewt-format); files to skip are listed in
  [`.mynewt-format-ignore`](../.mynewt-format-ignore).
- **`mynewt_style/`**: the engine, its rules (`mynewt_style/rules/`, see the
  [rules README](mynewt_style/rules/README.md)) and unit tests.
- **[`mynewt_style/CODING_STANDARDS.md`](mynewt_style/CODING_STANDARDS.md)**: the
  coding standard the tool checks, rule by rule.

- **`mynewt-format-setup.sh`**: installs the tool as a `mynewt-format`
  command, for use in other Mynewt repositories (see below).

mynewt-format lives only here. Other Mynewt repositories, such as
apache-mynewt-nimble, do not carry a copy: they keep just their own
`.mynewt-format` and `.mynewt-format-ignore`, and both developers and CI use
the tool from mynewt-core. Keep the rules repository neutral; anything
specific to one code base belongs in that repository's `.mynewt-format`.

## mynewt-format.py

Requires Python 3.11+ and nothing else. Unlike a whole-file formatter it only
touches code that breaks a rule, so hand-made line breaks and aligned blocks
stay as they are.

```bash
python3 tools/mynewt-format.py                 # check what you changed (working tree)
python3 tools/mynewt-format.py -d               # show proposed fixes as a diff
python3 tools/mynewt-format.py -i               # review each fix: y/n/a/s/A/q
python3 tools/mynewt-format.py -f               # apply all fixes
python3 tools/mynewt-format.py --staged         # check what is staged for commit
python3 tools/mynewt-format.py --git-diff origin/master
                                                # check what changed on this branch (CI)
python3 tools/mynewt-format.py path/to/file.c   # changed lines of given files/dirs
python3 tools/mynewt-format.py --whole-files path/to/file.c
                                                # every line of given files/dirs
python3 tools/mynewt-format.py --all            # every line of the whole repository
python3 tools/mynewt-format.py --report list    # one line per problem (for editors, scripts)
python3 tools/mynewt-format.py --list-rules     # every rule, -v for full details
python3 tools/mynewt-format.py --explain define-alignment
python3 tools/mynewt-format.py --install-hook   # pre-commit check of staged lines
```

By default only lines changed according to git are reported and fixed (new
files completely), so older code that predates a rule does not get in the
way; `--whole-files` or `--all` check everything. Each problem is shown with
its source line and, when it can be fixed, the proposed fix right below it;
`--report list` prints one `file:line:col` line per problem instead. CI runs it the same way on
every pull request as an informational check: it prints problems and proposed
fixes, and leaves a review comment on each changed line that breaks a rule,
naming the rule (`.github/workflows/mynewt-format-check.yml` and
`mynewt-format-comments.yml`). It does not
fail the build (clang-format remains the blocking style check for now). To silence a rule locally, use a comment:

```c
x = foo;       /* mynewt-format: ignore line-length */
/* mynewt-format: ignore-next-line */
/* mynewt-format: off */  ...  /* mynewt-format: on */
```

Existing `/* clang-format off */` ... `/* clang-format on */` regions are
honoured too.

The exit status is 0 when clean, 1 when problems were found (or `-d` proposes
changes), and 2 on usage or configuration errors. `--format github` prints
GitHub Actions annotations; `--format json` prints one JSON object per problem
(path, line, column, rule, severity, message, fixable, source).

### Using it in other repositories (NimBLE)

The tool picks up the `.mynewt-format` of the repository you run it in, so
one copy serves every repository. In a newt project mynewt-core is already
checked out next to NimBLE:

```bash
cd repos/apache-mynewt-nimble
python3 ../apache-mynewt-core/tools/mynewt-format.py
```

To get a `mynewt-format` command (a link in `~/.local/bin` to this checkout,
so it follows `git pull`) and, optionally, a pre-commit hook in NimBLE:

```bash
/path/to/mynewt-core/tools/mynewt-format-setup.sh --hook /path/to/mynewt-nimble
mynewt-format -i                                # in any Mynewt repository
```

Or just add an alias to `~/.bashrc`:

```bash
alias mynewt-format='python3 /path/to/mynewt-core/tools/mynewt-format.py'
```

In CI, other repositories call the reusable workflows of mynewt-core, which
fetch the tool from mynewt-core's `master`:

```yaml
# in the pull_request workflow (e.g. compliance_check.yml)
jobs:
  style_check_mynewt_format:
    uses: apache/mynewt-core/.github/workflows/mynewt-format-check.yml@master
```

and a separate workflow posts the review comments; see the header of
`.github/workflows/mynewt-format-comments.yml`.

Tests: `python3 -m unittest discover -s tools/mynewt_style/tests`
