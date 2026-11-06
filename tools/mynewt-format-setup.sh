#!/bin/sh
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

# Makes this checkout's mynewt-format available as the 'mynewt-format'
# command, so it can be used in any Mynewt repository (e.g. NimBLE), which
# only carries its own .mynewt-format configuration.
#
#   tools/mynewt-format-setup.sh                   install the command
#   tools/mynewt-format-setup.sh --hook DIR        also add a pre-commit hook
#                                                  to the git repository DIR
#   tools/mynewt-format-setup.sh --bin-dir DIR     install into DIR
#                                                  (default: ~/.local/bin)

set -e

TOOL="$(cd "$(dirname "$0")" && pwd -P)/mynewt-format.py"
BIN_DIR="$HOME/.local/bin"
HOOK_REPOS=""

usage() {
    sed -n '/^# Makes/,/^$/s/^# \{0,1\}//p' "$0"
    exit "$1"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --bin-dir) [ -n "$2" ] || usage 2; BIN_DIR="$2"; shift 2 ;;
        --hook) [ -n "$2" ] || usage 2; HOOK_REPOS="$HOOK_REPOS
$2"; shift 2 ;;
        -h|--help) usage 0 ;;
        *) echo "unknown option: $1" >&2; usage 2 ;;
    esac
done

if [ ! -f "$TOOL" ]; then
    echo "error: $TOOL not found" >&2
    exit 1
fi

mkdir -p "$BIN_DIR"
ln -sf "$TOOL" "$BIN_DIR/mynewt-format"
echo "installed $BIN_DIR/mynewt-format -> $TOOL"

case ":$PATH:" in
    *":$BIN_DIR:"*) ;;
    *)
        echo ""
        echo "$BIN_DIR is not in your PATH; add this to ~/.bashrc:"
        echo "    export PATH=\"$BIN_DIR:\$PATH\""
        ;;
esac

echo "$HOOK_REPOS" | while IFS= read -r repo; do
    [ -n "$repo" ] || continue
    (cd "$repo" && python3 "$TOOL" --install-hook)
done
