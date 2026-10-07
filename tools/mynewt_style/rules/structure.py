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

"""File structure: include guards, C++ wrappers."""

import re

from mynewt_style import FileContext, Rule, register_rule


@register_rule
class HeaderGuard(Rule):
    name = "header-guard"
    category = "structure"
    summary = "Headers are protected by a matching #ifndef/#define guard (or #pragma once)."
    explanation = """
        CODING_STANDARDS.md: header files contain "#ifdef aliasing, to prevent
        multiple includes":

            #ifndef H_BLE_FOO_
            #define H_BLE_FOO_
            ...
            #endif

        The first directive of the header must be '#ifndef X' immediately
        followed by '#define X' with the *same* X (a typo there silently
        disables the guard). '#pragma once' is accepted as well.
    """
    severity = "warning"

    def check(self, ctx: FileContext):
        if not ctx.is_header:
            return []
        directives = ctx.directives
        if not directives:
            return [self.violation(ctx, 1, 1, "header has no include guard")]
        first = directives[0]
        text = ctx.lines[first.start_line - 1].strip()
        if re.match(r"#\s*pragma\s+once\b", text):
            return []
        m = re.match(r"#\s*(?:ifndef\s+(\w+)|if\s+!\s*defined\s*\(?\s*(\w+))", text)
        if not m:
            return [self.violation(ctx, first.start_line, 1,
                                   "first directive of a header must start the include guard")]
        guard = m.group(1) or m.group(2)
        if len(directives) < 2:
            return [self.violation(ctx, first.start_line, 1, "include guard has no #define")]
        second = directives[1]
        m2 = re.match(r"#\s*define\s+(\w+)", ctx.lines[second.start_line - 1].strip())
        if not m2 or m2.group(1) != guard:
            got = m2.group(1) if m2 else ctx.lines[second.start_line - 1].strip()
            return [self.violation(ctx, second.start_line, 1,
                                   f"include guard mismatch: '#ifndef {guard}' followed by "
                                   f"'{got}'")]
        last = directives[-1]
        if last.name != "endif":
            return [self.violation(ctx, last.start_line, 1,
                                   "last directive of a guarded header should be #endif")]
        return []


@register_rule
class ExternC(Rule):
    name = "extern-c"
    category = "structure"
    summary = "Public headers wrap their declarations in extern \"C\" for C++ users."
    explanation = """
        CODING_STANDARDS.md: public headers carry the C++ wrapper after their
        #include lines:

            #ifdef __cplusplus
            extern "C" {
            #endif

        Only headers below an 'include/' directory are checked.
    """
    severity = "warning"

    def check(self, ctx: FileContext):
        if not ctx.is_header or "include" not in ctx.path.parts:
            return []
        if "__cplusplus" in ctx.content and re.search(r'extern\s+"C"', ctx.content):
            return []
        return [self.violation(ctx, 1, 1, "public header lacks the extern \"C\" wrapper")]
