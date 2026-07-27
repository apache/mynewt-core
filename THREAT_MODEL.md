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

# Apache Mynewt (mynewt-core) Security Threat Model (draft)

> **Status: v0 draft for PMC review.** Drafted by the ASF Security team
> from the public repository layout and general embedded-RTOS domain
> knowledge, against the Scovetta rubric. `mynewt-core` is **large** (a
> whole RTOS: kernel, HAL/BSPs, network stacks, crypto, bootloader
> integration, filesystems, management), so this v0 is deliberately an
> **umbrella** that draws the trust model once and then maps each
> component family to it — with a correspondingly high number of §14
> questions where only a maintainer can confirm a per-subsystem detail.
> Provenance tags: *(documented)* / *(maintainer — none yet)* /
> *(inferred)*. A starting point to react to, not a finished model.

## §1 Header

- **Project:** Apache Mynewt — a real-time operating system for
  constrained 32-bit embedded devices ("build, deploy and securely manage
  billions of devices") *(documented: README)*.
- **Repository / commit:** `apache/mynewt-core`, `master` @ `1dcb119ed885` (2026-06-09).
- **Drafted:** 2026-06-13, ASF Security team (v0 draft from public artefacts).
- **Companion repos this round:** `apache/mynewt-nimble` (the BLE stack —
  its own model covers the radio surface) and `apache/mynewt-mcumgr` (the
  SMP management library — its own model covers the SMP surface).
  `mynewt-core` **hosts both**: it runs NimBLE and it ships its *own*
  management stack under `mgmt/` (`newtmgr`, `smp`, `oicmgr`, `imgmgr`)
  that is the same family as mcumgr. Where a surface is fully modelled in
  a sibling document, this model points there rather than duplicating.
- **What triggers a revision:** a new network stack or protocol under
  `net/`; a new management transport or command group under `mgmt/`; a
  change to the boot/image-validation integration under `boot/`; a change
  to which crypto backend is default; or a new externally-reachable
  parser. Internal kernel/driver refactors that do not change a
  wire-facing surface do not.

## §2 Scope and intended use

Mynewt is **an RTOS a product team builds their firmware on**. The
"caller" is the firmware author (trusted); the *adversary* reaches the
device only through whatever **externally-facing surface** the firmware
exposes — a network stack, a management transport, a console, a radio, a
sensor, or on-flash data. The whole of Mynewt runs in **one trust domain**
(single address space, typically no MMU-backed isolation), so the model is
organised around **which subsystems parse untrusted input** and what
happens when they do.

### Component families

| Family | Source | Externally reachable? | Modelled here as |
| --- | --- | --- | --- |
| **Kernel** | `kernel/os` (scheduler, mempool, mbuf, mutex/sem, callout), `kernel/sim` | only *indirectly*, via input flowing up from net/mgmt | trusted core; a kernel memory bug reachable from untrusted input is critical (§8.1) |
| **Network stacks** | `net/ip`, `net/oic` (OIC/CoAP), `net/lora` (LoRaWAN), `net/mqtt`, `net/wifi`, `net/cellular`, `net/osdp` | **yes — primary remote surface** | each is an untrusted-wire/radio parser (§6) |
| **Management** | `mgmt/{newtmgr,smp,oicmgr,imgmgr,mgmt,image_header}` | **yes** | same family as `apache/mynewt-mcumgr`; **see that model** for the SMP trust analysis. No authn/authz by design (§9) |
| **Crypto** | `crypto/mbedtls` (**vendored upstream**) | as a library, by the code above | primitives; mbedtls security is upstream's (§3, §11a). tinycrypt was **removed** from core before the 1.15 releases *(maintainer)* |
| **Boot / image** | `boot/{split,split_app,startup,stub}`, `mgmt/image_header` | at boot, over the staged image | image *authenticity* gate is the signature-verifying bootloader (MCUboot); see §9/§10 |
| **Sys / local mgmt** | `sys/{console,shell,config,log,coredump,fault,reboot,mfg,flash_map,stats,id}` | `console`/`shell` = local serial surface; also reachable remotely via `shell_mgmt` over SMP | local management surface (§6, §11) |
| **Filesystems** | `fs/*` (nffs, fatfs, littlefs ports) | via on-flash structures / file content | on-flash parser robustness (§6) |
| **Encoding** | `encoding/*` (tinycbor, json, base64, …) | by every layer that decodes untrusted data | untrusted-deserialization primitives (§6) |
| **HAL / BSP / drivers** | `hw/*` | sensor/peripheral input | per-driver. **Upstream-supported in-tree BSPs/drivers are in scope**; board hardware itself and out-of-tree BSPs are not (§3.3) *(maintainer)* |
| **Ships-but-context-dependent** | `apps/`, `test/`, `targets/`, demos | — | the integrator's / test code, not the library's runtime surface (§3) |

### What Mynewt is *not*

- Not a multi-tenant or process-isolated OS. There is generally **no MMU
  protection between components**; all firmware shares one address space
  and one trust level. *(maintainer — §14 Q1, confirmed)*
- Not the owner of image authenticity — that is the signature-verifying
  bootloader's (MCUboot). `boot/` here is the integration/split-image
  glue. *(maintainer — §14 Q2, confirmed)*
- Not the vendor of the third-party code it **vendors in-tree**. That is
  far wider than crypto: any package whose `pkg.yml` carries a
  `repository.<name>` stanza pulls its source from an upstream repo at
  `newt upgrade` time. Examples across families: `crypto/mbedtls`,
  networking (`blues-note-c`, `lwip`, `wiznet`, `osdp`), filesystems
  (`littlefs`), encoding (`nanopb`), plus a range of vendor SDKs and
  drivers (e.g. `lvgl`). *(maintainer)*

## §3 Out of scope (explicit non-goals)

1. **Vendored third-party code — all of it, not only crypto.** A package
   vendors upstream source when its `pkg.yml` declares a
   `repository.<name>` stanza; `newt upgrade` then fetches that source
   from the upstream repo. This covers `crypto/mbedtls`, networking
   (`blues-note-c`, `lwip`, `wiznet`, `osdp`), filesystems (`littlefs`),
   encoding (`nanopb`), and a range of vendor SDKs and drivers (e.g.
   `lvgl`). A CVE **inside** any such vendored component is upstream's,
   surfaced through Mynewt's dependency-update process — **not** a Mynewt
   threat-model finding, *unless* Mynewt **misconfigures** it (weak cipher
   defaults, disabled verification, an unsafe build-time option) — that
   misconfiguration **is** in model. *(maintainer — §14 Q3)*
2. **The application firmware and its choices** — which network services
   it exposes, which management transports it enables, whether it requires
   a secure bootloader. Trusted/owned by the integrator (§10).
3. **Board *hardware* characteristics** — the integrator selects the
   board; physical and electrical properties are out, as are
   out-of-tree/vendor BSPs the project does not carry.
   **In scope, however: the upstream-supported BSPs and drivers carried
   in-tree under `hw/`.** A memory-safety or bounds failure in an in-tree
   driver that parses sensor/peripheral input is a VALID finding on the
   same footing as a `net/` parser. *(maintainer — §14 Q16)*
4. **`apps/`, `test/`, demo targets** — example and test code, not the
   library's production runtime.
5. **The SMP/management *protocol* trust analysis** — fully covered in
   `apache/mynewt-mcumgr`'s model; not duplicated here (this model only
   notes that `mgmt/` is the same family and inherits its §9).
6. **The BLE radio surface** — covered in `apache/mynewt-nimble`'s model.
7. **Physical / invasive / side-channel and supply-chain** concerns.

## §4 Trust boundaries and data flow

One boundary, many ingress points. Untrusted input enters Mynewt through
several doors; all of them cross into the **single trusted firmware
domain**:

```
   UNTRUSTED INGRESS                         |  TRUSTED (single address space)
                                             |
  network/radio peer ── net/ip,oic,lora,     |
                         mqtt,wifi,cellular,  |── parse ──┐
                         osdp ───────────────>|           │
  mgmt client (SMP/      mgmt/{smp,newtmgr,   |           │
   newtmgr/oic) ───────  oicmgr,imgmgr} ─────>|── parse ──┤
  local operator ─────── sys/console, shell ─>|           ├─> kernel objects,
  on-flash data ──────── fs/*, mgmt/image_hdr>|── parse ──┤    flash, reset,
  sensor/peripheral ──── hw/* drivers ───────>|           │    config, files
                                             |           │
                       encoding/* (cbor/json/base64) ─────┘  (no MMU boundary
                                             |                between any of these)
```

Because there is no internal isolation, **the robustness of every
front-line parser is the whole device's robustness**. The security of a
*deployed* device is the union of: Mynewt's parser/kernel correctness
(this model) + the integrator's choices about which doors to open and how
to gate them (§10) + the sibling models for BLE (nimble) and SMP (mcumgr).

## §5 Assumptions about the environment

- **Single address space, generally no MMU-enforced isolation**; a
  memory-safety violation in any reachable parser is whole-device.
  *(inferred — §14 Q1)*
- **A signature-verifying bootloader (MCUboot) gates image execution** if
  the product needs firmware-update integrity; Mynewt stages, MCUboot
  verifies. *(inferred — §14 Q2)*
- **The HAL/flash/RNG provided by the selected BSP behave correctly**;
  Mynewt's guarantees are conditional on them. In particular the **RNG
  feeding any crypto / management nonce is CSPRNG-quality**. *(inferred —
  §14 Q4)*
- **The firmware author is trusted** and configures the exposed surface.

## §5a Build-time and configuration variants

Mynewt is a **syscfg-composed** system: almost every subsystem is
opt-in/opt-out at build time, and the security posture is defined by the
selection. Load-bearing variants:

- **Which `net/` stacks are linked in** (an IP-less BLE-only build has no
  IP attack surface; an OIC/CoAP or LoRaWAN build does). *(documented:
  per-package `syscfg.yml`)*
- **Which `mgmt/` transports/groups are enabled** (newtmgr vs SMP vs
  oicmgr; `shell_mgmt` reachable remotely). Inherits mcumgr's §5a.
- **Crypto backend** (`crypto/mbedtls`) and its cipher/verification
  configuration. *(maintainer — §14 Q3; tinycrypt is gone from core)*
- **RNG source.** Mynewt provides **no unified random API**. Consumers use
  either libc's, or — where the platform has one — a TRNG driver under
  `hw/drivers/trng/`. CSPRNG quality is therefore a **per-platform /
  integrator** property, not something core guarantees.
  *(maintainer — §14 Q4)*
- **Secure-boot integration** (`boot/` split/stub) and whether image
  signature verification is enabled. *(maintainer — §14 Q2, confirmed)*
- **`sys/shell` over console and/or over mgmt** — a powerful local/remote
  surface when enabled. *(documented: `sys/shell`, `cmd/shell_mgmt`)*
- **`sys/coredump` / `sys/fault`** may expose memory state; they are
  intentional diagnostics for the integrator to gate.
  *(maintainer — §14 Q5, confirmed)*

## §6 Assumptions about inputs

Untrusted input arrives at many parsers; each must be memory-safe and
bounded against an adversary who controls **every byte**. The table maps
the families to their obligation (depth per subsystem is a §14 item —
this v0 cannot have read every parser).

| Ingress | Source | Adversary reach | Must enforce |
| --- | --- | --- | --- |
| IP / UDP / TCP frames | `net/ip` | network peer | memory-safe header/option parsing; bounded reassembly *(inferred — §14 Q6)* |
| **OIC / CoAP** requests | `net/oic` | network peer | safe CoAP option/payload parse; the OIC mgmt path (`oicmgr`) inherits mgmt's no-auth posture *(inferred — §14 Q7)* |
| **LoRaWAN** join/data | `net/lora` | anyone in radio range | MIC/join-nonce handling; safe MAC-command parse; replay posture per spec *(inferred — §14 Q8)* |
| MQTT broker responses | `net/mqtt` | the broker / a MITM | safe CONNACK/PUBLISH/variable-length parse *(inferred — §14 Q9)* |
| **OSDP** messages | `net/osdp` | a peer on the RS-485/serial bus (physical-access-control context!) | safe message parse; SCBK/secure-channel correctness *(inferred — §14 Q10)* |
| SMP / newtmgr / oic mgmt frames | `mgmt/*` | a peer on the mgmt transport | **see `apache/mynewt-mcumgr` §6**; no authn/authz by design |
| Console / shell input | `sys/console`, `sys/shell` | local serial (or remote via `shell_mgmt`) | bounded line handling; shell is powerful-by-design (§11) |
| On-flash filesystem structures | `fs/*` | whoever can write flash / supply an image | safe parse of corrupt/hostile on-flash metadata *(inferred — §14 Q11)* |
| CBOR / JSON / base64 payloads | `encoding/*` | every caller above | bounded, memory-safe decode of malicious encodings *(inferred — §14 Q12)* |
| Staged firmware image | `mgmt/image_header`, `boot/` | the mgmt peer | safe image-header parse; **execution gated by MCUboot signature check, not by Mynewt** *(inferred — §14 Q2)* |

### Size / shape / rate

- Allocation in the constrained device is bounded by mempool/mbuf pool
  sizing (`kernel/os`); a parser that lets a lying length field exceed a
  pool or a stack buffer is the canonical embedded bug. *(inferred — §14 Q13)*
- **Heap allocation is also available** — via the system's `os_malloc` or
  libc's `malloc` directly — so mempool/mbuf sizing is not the only bound
  in play. Heap exhaustion and heap-overflow reachable from untrusted
  input are in scope on the same footing as the pool cases.
  *(maintainer — §14 Q13)*
- No general rate-limit/DoS guarantee against a peer who can reach a
  network/mgmt surface. *(inferred — §14 Q14)*

## §7 Adversary model

| Actor | In scope? | Capabilities |
| --- | --- | --- |
| **Network/radio peer reachable by an enabled `net/` stack** | **yes — primary remote** | craft arbitrary protocol frames (IP/CoAP/LoRa/MQTT/OSDP/Wi-Fi); fuzz; replay; flood. |
| **Peer on an enabled `mgmt/` transport** | **yes** | full SMP/newtmgr/oic mgmt surface — see mcumgr model; no auth by design. |
| **Local operator at the console / shell** | **yes for robustness; powerful by design** | the shell is intended to be powerful; the question is parser safety, not "the shell can do things". |
| **Supplier of on-flash data / a staged image** | **yes for parser robustness** | corrupt FS metadata or a malformed image header → the parsers must not be exploitable; image *execution* is MCUboot-gated. |
| **A compromised in-firmware component** | **out of scope** | all firmware is one trust domain; Mynewt does not defend a component from another. |
| **The integrator / firmware author** | **out of scope** | trusted. |
| **Vendored-mbedtls internal flaw** | **out of scope** (upstream) | unless Mynewt misconfigures it (then in scope). |
| **Physical / side-channel / RF-layer attacker** | **out of scope** | §3, §5a-coredump aside. |

### Amplifier

The **no-MMU single-address-space** property (§5) means there is no
"contained" memory bug: any reachable OOB write in any parser is a
candidate for full device control (subject only to the absence of an
exploit mitigation the platform may or may not provide). This raises the
severity floor for every §6 parser finding.

## §8 Security properties the project provides

For each: condition, violation symptom, severity, provenance. Mynewt is
plumbing, so the properties are mostly *robustness* properties; the
*policy* is the integrator's.

1. **Memory-safe, bounded handling of malformed input in every
   externally-reachable parser** (net/, mgmt/, fs/, encoding/, image
   header), given a correct BSP. *(inferred — §14 Q6–Q13; foundational)*
   - *Violation:* a crafted frame/file/encoding → OOB read/write, stack
     overflow, unbounded allocation, or infinite loop.
   - *Severity:* **high** (no isolation; §7 amplifier).
2. **Kernel object integrity under valid use** (scheduler, mempool, mbuf,
   sync primitives) — no corruption from well-formed concurrent use.
   *(inferred — §14 Q15)*
   - *Violation:* a use-after-free / double-free / race in mbuf or mempool
     reachable from the input path.
   - *Severity:* **high**.
3. **Correct *use* of the crypto primitives Mynewt configures** (sane
   cipher selection, verification enabled where Mynewt sets defaults).
   *(inferred — §14 Q3)*
   - *Violation:* Mynewt selecting a broken cipher default or disabling a
     verification it should leave on.
   - *Severity:* **high** (but the primitive itself is upstream's).
4. **Correct image-header parsing and slot handling** so a malformed
   staged image cannot corrupt the device before MCUboot ever evaluates
   it. *(inferred — §14 Q2)*
   - *Violation:* image-header parse overflow.
   - *Severity:* **high**.

## §9 Security properties the project does *not* provide

- **No process / memory isolation between firmware components.** *(inferred
  — §14 Q1)*
- **No authentication/authorization at the management layer** (`mgmt/`) —
  identical to mcumgr; the transport and bootloader are the gates. *(see
  mcumgr §9)*
- **No image authenticity guarantee** — that is the signature-verifying
  bootloader's (MCUboot). Mynewt stages bytes. *(inferred — §14 Q2)*
- **No guarantee for vendored-mbedtls internal correctness** — upstream's
  (§3.1, §11a).
- **No availability/DoS guarantee** against a peer who can reach a
  network/mgmt surface. *(inferred — §14 Q14)*
- **No protection of secrets against a local operator with shell/console
  or coredump access**, when those are enabled. *(inferred — §14 Q5)*
- **No constant-time / side-channel guarantee** beyond what the chosen
  crypto backend provides. *(inferred)*

### False friends

- **`boot/` is not secure boot by itself** — the signature check is the
  bootloader's; `boot/` here is split-image/startup glue.
- **A `net/` stack accepting a connection is not authorization** — the
  service the app exposed is the app's policy choice.
- **`mgmt/smp` looks like secure management; it is not** — see mcumgr.

## §10 Downstream responsibilities

1. **Enable a signature-verifying bootloader (MCUboot)** and keep image
   verification on, so a staged image cannot execute unless signed.
2. **Expose only the `net/` stacks and `mgmt/` transports the product
   needs**, and gate each (link encryption, network segmentation, BLE
   bonding via nimble) per §10 of the relevant sibling model.
3. **Keep `sys/shell` / `shell_mgmt` / `coredump` out of production**
   unless required and access-gated.
4. **Track upstream advisories for every vendored component** — not just
   mbedtls, but each package pulled in through a `repository.<name>`
   stanza (`lwip`, `littlefs`, `nanopb`, `wiznet`, `osdp`, vendor SDKs
   and drivers …) — and update the vendored copies. *(maintainer)*
5. **Provide a CSPRNG-quality RNG** for any crypto/mgmt use. Core exposes
   **no unified random API** — you are choosing between libc's and a
   platform TRNG driver (`hw/drivers/trng/`), so the quality of what your
   crypto consumes is your decision. *(maintainer — §14 Q4)*
6. **Size mempool/mbuf pools** so an input flood degrades gracefully.

## §11 Known misuse patterns

- Shipping with `shell_mgmt`/`sys/shell` reachable over an open transport.
- Leaving image-signature verification off "to make updates easier".
- Exposing an OIC/CoAP or MQTT service on an open network and assuming the
  protocol authenticates the peer.
- Treating `boot/` as if it were the signature check.
- Carrying stale vendored code with known CVEs — mbedtls, but equally
  `lwip`, `littlefs`, `nanopb`, `wiznet`, `osdp`, `blues-note-c` or a
  vendor SDK/driver pulled in via a `repository.<name>` stanza.
  *(maintainer)*
- Enabling `sys/coredump` in production and leaking memory to whoever can
  pull it.

## §11a Known non-findings (recurring false positives)

| Reported as | Why it is a non-finding | Cite |
| --- | --- | --- |
| "CVE-XXXX in a vendored component" (`crypto/mbedtls`, `lwip`, `littlefs`, `nanopb`, `wiznet`, `osdp`, `blues-note-c`, a vendor SDK/driver …) | Vendored upstream source — any package with a `repository.<name>` stanza in its `pkg.yml`. Tracked via dependency update, not a Mynewt design finding — *unless* Mynewt misconfigures it. | §3.1, §10.4 |
| "`mgmt/smp` / `newtmgr` has no authentication" | By design — same as mcumgr; the transport + bootloader are the gates. | §9, mcumgr §9 |
| "Unauthenticated firmware update" | Intended; execution gated by MCUboot signature verification. | §9, §10.1 |
| "`sys/shell` allows arbitrary commands" | Powerful by design; build-gated; intended for trusted/local or dev contexts. | §5a, §11 |
| "A `net/` service is reachable without auth" | The app's service-exposure/policy choice, not a Mynewt-core bug — unless a parser is memory-unsafe (then VALID). | §9 false-friends |
| "LoRaWAN/OSDP/CoAP spec-level weakness" | A property of the protocol spec; in model only if Mynewt's *implementation* is memory-unsafe or deviates from the spec's security-relevant requirements. | §3, §6 |
| "`boot/` doesn't verify signatures" | Correct — that is MCUboot's job; `boot/` is split/startup glue. | §9 false-friends |
| "Coredump/fault output leaks memory" | Intended diagnostic; gate or disable in production (integrator). | §9, §10.3 |

Discriminator: **vendored-upstream issues, protocol-spec weaknesses, and
"no auth on a management/network surface"** are out of model; a
**memory-safety/bounds/concurrency failure in a reachable parser or kernel
object**, or a **Mynewt-level crypto *misconfiguration***, is VALID.

## §12 Conditions that would change this model

- A new `net/` stack or a new `mgmt/` transport/group.
- Taking ownership of image-signature verification (today MCUboot's).
- Switching the default crypto backend or its configuration.
- Introducing MMU/MPU-backed isolation between components (would weaken
  the §7 amplifier and change severities).
- A new externally-reachable parser anywhere in-tree.

## §13 Triage dispositions

| Disposition | Use when |
| --- | --- |
| **VALID** | Memory-unsafety / bounds / unbounded-resource / concurrency failure in any externally-reachable parser (`net/`, `mgmt/`, `fs/`, `encoding/`, image header) or kernel object reachable from input; a Mynewt-level crypto misconfiguration. |
| **OUT-OF-MODEL** | Depends on the absence of mgmt/network auth, on a protocol-spec weakness, on an internal flaw in **any vendored component** (§3.1), or on a malicious-but-well-formed management command. |
| **DOWNSTREAM** | Fix is the integrator's: enable secure boot, gate/disable a surface, update a vendored component, size pools, supply a CSPRNG-quality RNG (core provides no unified random API — §5a). |
| **NON-FINDING** | Matches a §11a row. |
| **MODEL-GAP** | Real, in-scope in spirit, no §8/§9 item covers it → §14. |

## §14 Open questions for the maintainers

Answered by Szymon Janc (Mynewt PMC) on 2026-07-27, in review of the PR
that introduced this document. Answered items are folded into the body
above and retained here with their answers; the remainder stay open.

**Architecture / trust — ANSWERED**
- **Q1.** No-MMU single-address-space / no-inter-component isolation.
  → **Confirmed.** Sets the §7 severity amplifier.
- **Q2.** Boot/image responsibility split — MCUboot's signature
  verification is the sole execution gate. → **Confirmed.**
- **Q4.** RNG consumed by crypto/management paths.
  → **There is no unified random API in the OS.** Consumers use libc's,
  or a platform TRNG driver under `hw/drivers/trng/`. CSPRNG quality is a
  per-platform / integrator property (§5a, §10).

**Crypto — ANSWERED**
- **Q3.** `crypto/mbedtls` vendoring and defaults; when is tinycrypt used?
  → **tinycrypt has been removed** from core (before the 1.15 releases),
  so the question is moot for it. mbedtls is vendored upstream, and the
  vendoring rule generalises to every package with a `repository.<name>`
  stanza in its `pkg.yml` (§3.1).

**Per-subsystem parser robustness** (each: what is the robustness target
against a fully-malicious peer, and has it been fuzzed?)
- **Q6.** `net/ip` · **Q7.** `net/oic` (CoAP) + `oicmgr` ·
  **Q8.** `net/lora` (LoRaWAN MIC/MAC) · **Q9.** `net/mqtt` ·
  **Q10.** `net/osdp` (secure-channel + RS-485 bus context) ·
  **Q11.** `fs/*` on-flash metadata · **Q12.** `encoding/*` (cbor/json/
  base64) · **Q13.** where lying length fields are bounded vs pools/stack.

**Kernel / resources**
- **Q15.** mbuf/mempool use-after-free / double-free / race posture on the
  input path.
- **Q14.** Any intended DoS/rate-limit posture, or wholly the integrator's.

**Sys surfaces — ANSWERED**
- **Q5.** Are `sys/coredump`/`sys/fault` intentional diagnostics for the
  integrator to gate? → **Yes** (§5a, §11a).

**Scope confirmation — ANSWERED**
- **Q16.** `apps/`, `test/`, demo `targets/`, `hw/bsp/*` out of model?
  → **Partly.** `apps/`, `test/` and demo `targets/` are out, but
  **upstream-supported BSPs and in-tree drivers under `hw/` are IN
  scope** (§3.3). The in-tree `mgmt/` surface is read against the
  `apache/mynewt-mcumgr` model rather than re-analysed here.

**Meta — ANSWERED**
- **Q17.** OK for this `THREAT_MODEL.md` to be the canonical model,
  reached via `AGENTS.md → SECURITY.md → THREAT_MODEL.md`?
  → **Yes** — that chain lands in this same PR.
- **Q18.** Single umbrella model, or per-`net/`-stack sub-models?
  → **"For now lets do single umbrella."** Revisit per §12 if a
  particular stack warrants its own model later.

**Still open** — the per-subsystem parser-robustness questions (Q6–Q13)
and the kernel/resource questions (Q14, Q15) above. Szymon noted some
touch very low-level details; they are not blocking, and the model is
usable without them. They stay listed so a future reader knows which
claims are still *(inferred)*.

## Appendix: existing security-policy artefacts → §x back-map

At `master @ 1dcb119ed885`, `apache/mynewt-core` contains **no
`SECURITY.md`, `AGENTS.md`, or prior threat-model document** at the root.
`CODING_STANDARDS.md` is a style guide, not a security policy. This is a
greenfield v0; there is nothing to supersede or back-map. Repository-
layout facts are cited inline as *(documented)*.

Claims about per-subsystem behaviour started as *(inferred)*, because a
light orient pass cannot responsibly assert the internals of a codebase
this size. Szymon Janc (Mynewt PMC) answered the architecture, crypto,
RNG, sys-surface, scope and meta questions on 2026-07-27; those claims are
now marked *(maintainer)* and, where his answer corrected the draft, the
body has been rewritten rather than annotated. The per-parser questions
(Q6–Q13, Q14, Q15) remain *(inferred)*.
