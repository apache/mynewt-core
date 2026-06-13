# Agent guidance

This file is read by automated agents (security scanners, code analyzers,
AI assistants) operating on this repository. It points them at the
human-authored references they should consult before producing output.

## Security

Security model: [SECURITY.md](./SECURITY.md), which links to the project's
threat model at [THREAT_MODEL.md](./THREAT_MODEL.md).

Apache Mynewt is a real-time operating system for constrained 32-bit
devices. Before reporting a security issue, consult `THREAT_MODEL.md`:
Mynewt runs in a single address space with no inter-component isolation, so
the in-model surface is memory safety in the externally-reachable parsers
(`net/`, `mgmt/`, `fs/`, `encoding/`) and kernel objects reachable from
input. Out of model: vendored Mbed-TLS internal CVEs (upstream's),
protocol-spec weaknesses, the no-authentication management surface (modelled
in `apache/mynewt-mcumgr`), and firmware-image authenticity (the
bootloader's). The BLE radio surface is modelled in `apache/mynewt-nimble`.
See the §11a known-non-findings list.
