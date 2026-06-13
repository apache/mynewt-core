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
