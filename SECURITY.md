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
#  KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
-->

# Security Policy

## Reporting a Vulnerability

Please report suspected security vulnerabilities privately to the Apache
Security Team at <security@apache.org>, following the ASF process at
<https://www.apache.org/security/>. Do not open public GitHub issues or
pull requests for security reports.

## Threat Model

Apache Mynewt's security threat model — its scope, trust boundaries, the
security properties it does and does not provide, the adversary model, and a
list of recurring non-findings — is documented in
[THREAT_MODEL.md](./THREAT_MODEL.md).

Key points for triagers and scanners: Mynewt runs in a single address space
with no inter-component isolation, so the in-model surface is memory safety
in the externally-reachable parsers (`net/`, `mgmt/`, `fs/`, `encoding/`)
and kernel objects. Out of model are vendored Mbed-TLS internal CVEs
(tracked via upstream), protocol-specification weaknesses, the
no-authentication management surface (see the `apache/mynewt-mcumgr` model),
and firmware-image authenticity (enforced by the signature-verifying
bootloader). The BLE radio surface is modelled in `apache/mynewt-nimble`.
See `THREAT_MODEL.md` §9 and §11a.
