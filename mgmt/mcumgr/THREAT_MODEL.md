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

# Apache mcumgr Security Threat Model (draft)

> **Status: v0, reviewed by the PMC.** This document was drafted by the
> ASF Security team from public artefacts (the in-repo `README.md`,
> `protocol.md`, `transport/*.md`, and the `cmd/*/syscfg.yml` defaults)
> against the Scovetta threat-model rubric. **Szymon Janc (Mynewt PMC)
> reviewed it on 2026-07-27** and answered the core-protocol, build-time,
> scope and meta questions in §14; his answers are folded into the body.
> Every non-trivial claim carries a provenance tag — *(documented)*
> (stated in the repo's own files), *(maintainer)* (ratified or corrected
> by a PMC member), or *(inferred)* (reasoned from code structure /
> domain knowledge, and routed to a numbered question in §14). The
> remaining *(inferred)* claims are the per-handler robustness questions;
> corrections are still welcome on any of them.
>
> **Moved into `apache/mynewt-core`.** This model was written for the
> `apache/mynewt-mcumgr` repository. mcumgr has since been moved into
> `apache/mynewt-core` (`mgmt/mcumgr/`) and its OS porting layer, Zephyr
> support and `shell_mgmt` were removed. Paths and porting-layer remarks
> below are updated accordingly; the trust analysis itself is unchanged.

## §1 Header

- **Project:** Apache mcumgr — a management library for 32-bit MCUs.
- **Repository / commit:** originally `apache/mynewt-mcumgr`, `master` @ `0b63bc54308b` (2026-05-20); now `apache/mynewt-core` `mgmt/mcumgr/`.
- **Drafted:** 2026-06-13, by the ASF Security team (v0 draft from public artefacts).
- **Companion models:** `apache/mynewt-core`'s top-level
  `THREAT_MODEL.md` (the RTOS mcumgr is now part of) and
  `apache/mynewt-nimble` (the BLE stack that provides one of mcumgr's
  transports). mcumgr is Mynewt-only and uses Mynewt APIs directly; Zephyr
  **forked mcumgr into its own tree**, so its divergences are out of model
  here. *(maintainer — §14 Q4, Q19)* Where a property is delegated to the
  host OS or the bootloader, that is called out explicitly.
- **What should trigger a revision of this model:** a new SMP command
  group; a new transport; the addition of any authentication /
  authorization layer to SMP; a change to which command groups are
  compiled in by default; or a change to how uploaded images are gated
  before execution. Internal refactors that do not change the wire
  protocol, the command surface, or the trust boundary do **not** require
  a revision.

## §2 Scope and intended use

mcumgr is **a library that a device-firmware author integrates into a
product**, not a standalone application. It implements the device-side
(**server**) half of **SMP — the Simple Management Protocol** — plus a
set of management command handlers and pluggable transports. The remote
(**client**) half is a separate tool (`mcumgr-cli` / `newtmgr`, in other
repos) or a mobile/desktop app; it is **out of this model's scope** (and
was explicitly declined from this review round by the PMC). *(documented:
README; maintainer scope decision on the PMC's private list)*

**Intended deployment.** mcumgr is compiled into the firmware image of an
embedded device and exposed over one or more transports so that an
operator (or a provisioning/field-update tool) can manage the device:
upload new firmware, inspect state, read/write files, reset it. The
canonical use is **device firmware update (DFU)** and field diagnostics.
*(documented: README, `mgmt/mcumgr/cmd/img_mgmt`)*

### Component families (each modelled at its own trust/surface level)

| Family | Directories | What it does | Surface |
| --- | --- | --- | --- |
| **SMP core** (frame + dispatch) | `mgmt/mcumgr/{smp,mgmt,omp}/`, Mynewt glue in `mgmt/smp/`, `mgmt/oicmgr/` | Parses the `mgmt_hdr` frame header, dispatches by `(group, id)`, decodes the CBOR payload, encodes the response. | The whole untrusted-input parse path — every byte after the transport hands a frame up. |
| **Command handlers** | `mgmt/mcumgr/cmd/{img,fs,os,log,stat}_mgmt`; other groups outside `mgmt/mcumgr/`: `sys/config` (config), `sys/shell` (`SHELL_BRIDGE`, shell exec), `mgmt/smp/smp_os`, `mgmt/imgmgr`, `test/crash_test`, `test/runtest` | Implement individual commands within their group. Each has a **very different** privilege footprint (firmware write vs. read a stat). | Per-command; see §6 / §11. |
| **Transports** | `mgmt/smp/transport/{ble,smp_shell,smp_uart}` (specs: `mgmt/mcumgr/docs/smp-bluetooth.md`, `mgmt/mcumgr/docs/smp-console.md`), OIC transports of `net/oic` for OMP | Carry SMP frames between client and server. | The reachability boundary — *who* can deliver a frame at all. |
| **CBOR / attr decode** | `encoding/cborattr/`, `encoding/tinycbor/` | Decodes attacker-controlled CBOR payloads into C structs. | Classic untrusted-deserialization surface. |
| **Ships-but-context-dependent** | `apps/smp_svr`, `apps/omp_svr` | Example apps. | Demo / integration code; modelled as the integrator's, not the library's, per §3. |

### What mcumgr is *not*

- It is **not** a secure channel. It provides **no** authentication,
  authorization, confidentiality, integrity, or replay protection at the
  SMP layer (see §9). *(maintainer — §14 Q1, confirmed)*
- It is **not** the thing that decides whether an uploaded image is
  allowed to *run*. mcumgr **stages** an image into a secondary slot; a
  separate bootloader (**MCUboot**, in `apache/mynewt-core`'s `boot/`)
  verifies the signature and decides whether to boot it. *(documented:
  README; §14 Q2)*
- It is **not** a portable library anymore — it is part of Mynewt core and
  uses Mynewt APIs (flash, mbufs, sysinit) directly. Zephyr forked mcumgr
  into its own tree. *(maintainer — §14 Q4)*

## §3 Out of scope (explicit non-goals)

1. **The SMP client** (`mcumgr-cli`, `newtmgr`, mobile apps). Separate
   repos; declined from this round. A finding that requires a malicious
   *client* is in-model only insofar as it is *the server* mishandling a
   crafted frame (that is §6, and in scope) — the client's own code is
   out.
2. **Transport link security** — BLE pairing/bonding/encryption, serial
   physical access control, UDP network reachability. That is the
   transport's and the integrator's responsibility (§10), and for BLE
   specifically it lives in `apache/mynewt-nimble`'s Security Manager,
   not here.
3. **Image authenticity / secure boot.** Whether an uploaded image is
   signed, and whether an unsigned image can execute, is **MCUboot's**
   job (`apache/mynewt-core` `boot/`). mcumgr's role ends at writing
   bytes to the staging slot. *(documented: README; §14 Q2)*
4. **The rest of the RTOS.** Flash drivers, the scheduler, the HAL, `os_`
   primitives — modelled in `apache/mynewt-core`'s top-level
   `THREAT_MODEL.md`. mcumgr assumes they are correct (§5).
   *(maintainer — §14 Q4)*
5. **Zephyr and its fork.** Zephyr maintains its own fork of mcumgr in
   the Zephyr tree. Zephyr-only features and divergences are out of model
   here.
   *(maintainer — §14 Q4, Q19)*
6. **The integrating firmware author** — trusted by construction. mcumgr
   has no concept of a "user"; it has a remote *peer* on a transport, and
   that peer's trust level is entirely a property of the transport the
   integrator chose to expose (§7).
7. **Physical / invasive attacks** — chip decap, fault injection, bus
   probing, glitching the boot. Out of model.
8. **Supply-chain / build hygiene** — action pinning, release signing,
   dependency currency. Not a threat-model concern.

## §4 Trust boundaries and data flow

There is exactly **one trust boundary that matters**: the point where a
transport hands an SMP frame up to the SMP core. Everything the firmware
already contains (handlers, the CBOR decoder, the flash routines) is in a
**single trust domain** — one address space, no privilege separation, no
MMU isolation between command groups *(inferred: embedded RTOS model —
§14 Q3)*.

```
            UNTRUSTED                          |   TRUSTED (firmware image)
                                               |
 attacker in radio range / on serial / on net |
        |                                      |
        v   raw SMP frame (mgmt_hdr + CBOR)    |
  [ transport RX ] --------------------------> | [ SMP core: parse mgmt_hdr ]
        (BLE / console / UDP)                  |        |
                                               |        v  dispatch (group,id)
                                               | [ command handler ]
                                               |   img/fs/os/log/stat/shell
                                               |        |
                                               |        v  decode CBOR payload
                                               | [ cborattr / tinycbor ]
                                               |        |
                                               |        v
                                               | flash write (image slot) /
                                               | file read-write / reset /
                                               | stats read / shell exec
```

The trust transition is **frame ingress**. After that point there is no
further internal boundary: a handler that the integrator compiled in can
do whatever that command does, to whoever can send the frame. The
security of the system therefore rests almost entirely on **who can put a
frame on an enabled transport** (§7, §10) — not on anything SMP itself
checks.

## §5 Assumptions about the environment

- **32-bit MCU, single physical address space, no process isolation, no
  user/kernel split.** A memory-safety violation in any handler is a
  whole-device compromise, not a contained one. *(inferred — §14 Q3)*
- **The Mynewt services mcumgr uses are correct** — flash read/write/erase,
  mbufs, the SMP transport driver. mcumgr's guarantees are conditional on
  these. *(§14 Q4)*
- **For image execution to be gated, MCUboot (or an equivalent
  signature-verifying bootloader) is present and configured to reject
  unsigned/untrusted images.** Without that, "upload an image" becomes
  "run arbitrary code" — but that is a deployment property, not an
  mcumgr property. *(documented: README; §14 Q2)*
- **The firmware author chose which transports to expose and which
  command groups to compile in.** mcumgr does not pick these. *(documented:
  per-package `syscfg.yml`)*

## §5a Build-time and configuration variants

mcumgr's attack surface is **defined at build time** by which command
groups are linked in and how they are configured. This section is
load-bearing: the same library is benign or dangerous depending on these
choices.

- **Which command groups are compiled in.** `img_mgmt`, `fs_mgmt`,
  `os_mgmt`, `log_mgmt`, `stat_mgmt` are separate packages; remote shell
  command execution is provided by `sys/shell` (`SHELL_BRIDGE`).
  Remote shell execution and `fs_mgmt` (arbitrary file read/write) are
  the highest-power groups and should be off in most production builds.
  **On Mynewt there is no meaningful "default" set.** What is compiled in
  follows from which packages the application — or a system component —
  pulls in, which is a per-sample configuration choice. The rule of thumb
  is that a group must be **explicitly enabled by the user**, either
  directly in the app or transitively by pulling a package that depends
  on it (enabling USB, for instance, may pull `img_mgmt`). So "is it on by
  default?" is the wrong question to ask of this repo; "which packages
  does this product's build pull in?" is the right one.
  *(maintainer — §14 Q5)*
- **Upload chunk buffers are stack-allocated.**
  `IMG_MGMT_UL_CHUNK_SIZE` and `FS_MGMT_UL_CHUNK_SIZE` default to **512**
  and the syscfg descriptions state "a buffer of this size gets
  allocated **on the stack** during handling of an upload." The handler
  must therefore validate the attacker-supplied chunk length against this
  bound *before* copying — a classic embedded stack-overflow sink if the
  check is wrong or missing. *(documented: `mgmt/mcumgr/cmd/img_mgmt/syscfg.yml`,
  `mgmt/mcumgr/cmd/fs_mgmt/syscfg.yml`; §14 Q6)*
- **`FS_MGMT_PATH_SIZE` (default 64)** bounds the file-path buffer for
  `fs_mgmt`. Path handling against this bound, and any path-traversal
  containment, is a per-build concern. *(documented: `mgmt/mcumgr/cmd/fs_mgmt/syscfg.yml`;
  §14 Q7)*
- **Frame endianness.** SMP adds optional little-endian support on top of
  NMP's mandatory big-endian header. Both decode paths are reachable.
  *(documented: `protocol.md`)*
- **`img_mgmt` "dummy header" / direct-image-upload toggles** appear in
  syscfg and change behaviour; their security relevance needs maintainer
  input. *(documented: `mgmt/mcumgr/cmd/img_mgmt/syscfg.yml`; §14 Q8)*

## §6 Assumptions about inputs

The **only** untrusted input is the **SMP frame** delivered by a
transport. If the attacker can reach an enabled transport, **every byte
of the frame is attacker-controlled** — header and payload alike.

### SMP frame header trust table (`struct mgmt_hdr`)

| Field | Meaning | Attacker-controllable? | Handler/core must enforce |
| --- | --- | --- | --- |
| `nh_op` (3 bits) | READ / WRITE / *_RSP | **yes** | reject/ignore response opcodes arriving at a server; route only valid ops *(inferred — §14 Q9)* |
| `nh_flags` | reserved | **yes** | defined-bit validation; ignore reserved bits safely *(documented: "TBD"; §14 Q9)* |
| `nh_len` | **claimed** payload length | **yes** | the claimed length must be validated against the *actually received* byte count and against buffer bounds before use — never trusted as the copy size *(inferred — §14 Q10)* |
| `nh_group` | command group selector | **yes** | dispatch only to a registered group; unknown group → clean error, not UB *(inferred — §14 Q11)* |
| `nh_seq` | sequence number | **yes** | no replay/ordering guarantee is claimed (§9); used only to correlate response *(documented: "TBD"; §14 Q12)* |
| `nh_id` | command within group | **yes** | dispatch only to a registered handler; unknown id → clean error *(inferred — §14 Q11)* |

### CBOR payload trust table (per high-value command)

| Command group → command | Attacker-supplied fields | What the handler does with them | Must enforce |
| --- | --- | --- | --- |
| `img_mgmt` upload | image `data` chunk, `off`(set), `len`, `sha` | copies `data` into a **stack** buffer, writes it to the staging flash slot at `off` | `len`/chunk-size validated vs `IMG_MGMT_UL_CHUNK_SIZE` *before* copy; `off` monotonic / bounded to slot size; **never** treat acceptance as "this image is trusted" (that is MCUboot, §3.3) *(documented + inferred — §14 Q6)* |
| `fs_mgmt` upload/download | file `name`/path, `off`, `data`, `len` | opens a file at `name`, reads/writes at `off` | path bounded to `FS_MGMT_PATH_SIZE`; **path-traversal containment** to an intended directory (or the integrator accepts full-FS access); chunk bound as above *(inferred — §14 Q7)* |
| `os_mgmt` taskstat / mpstat / datetime / reset | command params | returns task/memory diagnostics; performs a device reset | bounded output encoding; reset only after the documented delay *(documented: reset delay syscfg)* — note these **leak internal state** and **reboot the device** to anyone on the transport, by design *(inferred — §14 Q13)* |
| shell exec (`sys/shell`, `SHELL_BRIDGE`) | a shell command line | executes it in the device shell | **this is arbitrary command execution by design** when the group is compiled in; intended for trusted/dev contexts only *(inferred — §14 Q5/Q14)* |

### Size / shape / rate

- **CBOR depth/size.** The payload is CBOR decoded by the bundled
  tinycbor/`cborattr`. Robustness against deeply-nested, truncated, or
  oversized CBOR is the decoder's job; whether mcumgr caps nesting/size
  before decode is unknown to us. *(inferred — §14 Q15)*
- **No SMP-layer rate limiting.** mcumgr does not rate-limit inbound
  frames; backpressure is the transport's. *(inferred — §14 Q16)*

## §7 Adversary model

### Actors

| Actor | In scope? | Capabilities |
| --- | --- | --- |
| **Anyone who can deliver an SMP frame over an *enabled* transport** | **yes — the primary adversary** | craft arbitrary SMP frames (any group/id/len, any CBOR), replay, flood, fuzz. For BLE: in radio range (bounded by whatever link security the integrator enabled in nimble — possibly none). For serial: physical/console access. For UDP: network reachability. |
| **A malformed-but-deliverable frame from an otherwise "legitimate" client** (buggy or compromised management tool) | **yes — parser robustness must hold** | drives the server's parse/dispatch/decode path with adversarial bytes; in-model for memory safety, hang, unbounded stack/heap use. |
| **The transport's link-layer peer** (e.g. a paired BLE central) | **conditionally** | if the integrator required BLE bonding+encryption, the adversary is reduced to a *bonded* peer; if not, it is anyone in range. The *reduction* is the integrator's doing, not mcumgr's (§10). |
| **The integrating firmware author** | **out of scope** | trusted; chooses transports, enabled groups, and the bootloader policy. |
| **Physical-invasive attacker** (glitch, decap, bus probe) | **out of scope** | §3.6. |
| **Side-channel observer** | **out of scope** *(inferred — §14 Q17)* | no constant-time or side-channel-resistance claim is made by mcumgr. |

### The defining asymmetry

Because SMP performs **no authentication or authorization** (§9), the
adversary's power is **entirely determined by transport reachability and
the set of enabled command groups** — not by anything mcumgr checks. A
device that exposes `img_mgmt` + `fs_mgmt` + shell exec over an
unencrypted, un-bonded BLE connection is, by design, **fully controllable
by anyone in radio range**. This is not a vulnerability in mcumgr; it is
the integrator operating mcumgr outside its intended trust assumptions
(§10, §11). Conversely, a memory-safety bug in the frame parser **is**
mcumgr's, because it breaks even when the transport is perfectly secured.

## §8 Security properties the project provides

For each: condition, violation symptom, severity, provenance. The list is
deliberately short — mcumgr is a thin protocol server, and most security
properties of a deployed device are delegated to the transport, the
bootloader, and the integrator (§10).

1. **Memory-safe parsing and dispatch of SMP frames on the compiled-in
   command groups, given correct underlying Mynewt services.** *(inferred — §14 Q10/Q11; this is the single most important property to confirm or qualify)*
   - *Violation symptom:* a crafted frame (bad `nh_len`, oversized upload
     chunk, malformed CBOR, unknown group/id) causes out-of-bounds
     read/write, stack overflow, or controlled corruption.
   - *Severity:* **high** — single address space, so corruption is
     whole-device (§5).
2. **Bounded stack use during uploads, when chunk length is validated
   against `*_UL_CHUNK_SIZE`.** *(documented config + inferred enforcement — §14 Q6)*
   - *Violation symptom:* an upload chunk larger than the configured
     bound overruns the stack buffer.
   - *Severity:* **high**.
3. **Faithful implementation of the SMP wire format** (header layout,
   op/group/id semantics, CBOR encoding of responses). *(documented: protocol.md)*
   - *Violation symptom:* a response that misencodes lengths/IDs such
     that a conforming client mis-parses it.
   - *Severity:* **low–medium** (interop / client-side, mostly).
4. **Clean rejection of unknown groups/commands** (no dispatch into
   unregistered handlers). *(inferred — §14 Q11)*
   - *Violation symptom:* an unknown `(group,id)` reaches uninitialised
     function state.
   - *Severity:* **medium–high**.

mcumgr explicitly does **not** appear to claim graceful behaviour under
*resource exhaustion* (flooding) or any property about *malicious but
well-formed* commands (those are §9/§10, by design).

## §9 Security properties the project does *not* provide

This is the most important section for triage, because mcumgr **looks
like** a management/security feature and is routinely assumed to provide
properties it does not.

- **No authentication.** SMP carries no credential. The server does not
  verify *who* sent a frame. *(inferred — §14 Q1)*
- **No authorization.** There is no per-command permission model; if a
  group is compiled in, every command in it is available to every peer
  that can reach the transport. *(inferred — §14 Q1)*
- **No confidentiality.** SMP frames are plaintext; any payload secrecy
  is the transport's (e.g. BLE link encryption). *(inferred — §14 Q1)*
- **No integrity / no anti-tamper / no replay protection at the SMP
  layer.** `nh_seq` correlates responses; it is not a nonce. *(documented:
  "Seq: TBD"; §14 Q12)*
- **No guarantee that an accepted firmware image is authentic or
  bootable.** mcumgr writes bytes to a slot; **MCUboot** decides whether
  the image runs. An "unauthenticated firmware upload" finding against
  mcumgr is OUT-OF-MODEL — it is intended behaviour with the gate
  elsewhere (§3.3, §11a). *(documented: README; §14 Q2)*
- **No DoS resistance.** A peer that can reach the transport can flood,
  reset (`os_mgmt`), or wedge the device; mcumgr makes no availability
  guarantee against an on-transport adversary. *(inferred — §14 Q16)*
- **No constant-time / side-channel guarantees.** *(inferred — §14 Q17)*

### False friends

- **"SMP" / "management protocol" sounds authenticated.** It is not — it
  is a transport-agnostic RPC with no security layer of its own.
- **`img_mgmt` "image hash" (`sha`) is an integrity/identification aid,
  not an authenticity check.** The cryptographic *authenticity* gate is
  MCUboot's signature verification, separately. *(inferred — §14 Q2)*

## §10 Downstream responsibilities

The integrator — not mcumgr — owns device security. Concretely:

1. **Gate the transport.** Require BLE bonding + LE Secure Connections
   encryption (configured in nimble); restrict serial to
   physically-trusted access; firewall/authenticate any UDP transport.
   An open transport = full device control to anyone who can reach it.
2. **Compile in only the groups you need.** Disable shell exec
   (`SHELL_BRIDGE`) and
   `fs_mgmt` in production unless the product genuinely requires remote
   shell / file access. Treat them as debug-only.
3. **Enable secure boot.** Ensure MCUboot (or equivalent) is present and
   configured to **reject unsigned/untrusted images**, so `img_mgmt`
   upload cannot become code execution.
4. **Bound and rate-limit at the transport** if availability matters.
5. **Assume any reachable, enabled command is fully exercised by an
   adversary.** Threat-model the *product* on that basis.

## §11 Known misuse patterns

- Exposing `img_mgmt`/`fs_mgmt`/shell exec over an **unencrypted,
  un-bonded BLE** connection in a shipped product.
- Shipping with **shell exec (`SHELL_BRIDGE`) enabled** (remote arbitrary command
  execution) because it was handy in development.
- **Relying on mcumgr for access control** ("only our app speaks SMP") —
  any peer on the transport can speak SMP.
- **Assuming firmware upload needs a credential** — it does not; only
  MCUboot's signature check stands between an uploaded image and boot,
  and only if enabled.
- **Disabling MCUboot signature verification** for convenience while
  leaving `img_mgmt` reachable.
- Treating the `sha`/hash in image upload as an authenticity guarantee.

## §11a Known non-findings (recurring false positives)

These are the reports a scanner/fuzzer/researcher will most predictably
file against mcumgr that are **out of model by design**. Triagers should
close them with the cited section.

| Reported as | Why it is a non-finding | Cite |
| --- | --- | --- |
| "SMP endpoint has no authentication / authorization" | By design — authn/authz is not an mcumgr property; the integrator gates the transport. | §9, §10.1 |
| "Unauthenticated firmware update / DFU over BLE/serial" | Intended; image *execution* is gated by MCUboot signature verification, which is separate. | §3.3, §9 |
| "shell exec (`SHELL_BRIDGE`) allows arbitrary command execution" | Opt-in, build-time-gated debug feature for trusted contexts; not compiled in for hardened builds. | §5a, §6, §10.2 |
| "`os_mgmt` taskstat/mpstat leaks internal memory/task layout" | Diagnostic by design; same transport-trust assumption as every other command. | §6, §9 |
| "`os_mgmt` reset lets a peer reboot the device (DoS)" | No availability guarantee against an on-transport adversary is claimed. | §9 |
| "No replay protection — frames can be replayed" | Correct; `nh_seq` is a correlator, not a nonce; not claimed. | §9 |
| "SMP frames are sent in cleartext" | Confidentiality is the transport's job (BLE link encryption), not SMP's. | §9, §10.1 |
| "Image `sha` is not a real signature" | Correct — it is an identifier/integrity aid; authenticity is MCUboot's. | §9 false-friends |

Anything resting on **"…but there's no authentication/encryption"** is
almost certainly this row, not a bug — confirm the report instead
describes a *memory-safety* or *bounds* failure in the parse/dispatch/
decode path before treating it as valid (§13).

## §12 Conditions that would change this model

- Adding an authentication or authorization layer to SMP (would create
  real §8 properties and move several §9 items).
- A new transport whose default reachability differs (e.g. an always-on
  IP transport).
- A new command group, or making a currently-opt-in group (shell, fs)
  default-on.
- Moving image-signature verification *into* mcumgr (today it is
  MCUboot's).
- A change to where upload buffers live (stack → heap) or how chunk
  bounds are enforced.

## §13 Triage dispositions

| Disposition | Use when |
| --- | --- |
| **VALID** | Memory-unsafety, stack/heap overflow, OOB read/write, or unbounded resource use reachable by a crafted SMP frame on a compiled-in handler (§6, §8) — i.e. it breaks *even with the transport perfectly secured*. |
| **OUT-OF-MODEL** | The report depends on the absence of authn/authz/encryption/replay protection, or on a malicious-but-well-formed command (§9). These are intended; the gate is the transport / bootloader / build config. |
| **DOWNSTREAM** | The fix is the integrator's: enable bonding, disable a group, turn on MCUboot signature verification, firewall the transport (§10). |
| **NON-FINDING** | Matches a §11a row. |
| **MODEL-GAP** | Real and in spirit in-scope, but no §8 property or §9 disclaimer covers it yet → feeds a §14 question and a model update. |

## §14 Open questions for the maintainers

Grouped; each promotes an *(inferred)* claim above to *(maintainer)* once
answered. **Szymon Janc (Mynewt PMC) answered the following on
2026-07-27**; his answers are folded into the body above and retained
here.

**Core protocol / trust — ANSWERED**
- **Q1.** SMP provides no authentication, authorization, confidentiality,
  integrity or replay protection of its own; all delegated to the
  transport/integrator. → **Confirmed.**
- **Q2.** mcumgr stages an image; MCUboot's signature verification is the
  sole gate on execution. → **Correct.**
- **Q3.** Single-address-space / no privilege separation — a memory bug in
  any handler is whole-device. → **Confirmed.**

**Porting / environment — ANSWERED**
- **Q4.** Which porting-layer guarantees does mcumgr rely on?
  → **Scope this model to the Mynewt porting layer.** Zephyr forked
  mcumgr into its own tree and no longer uses this repository. The PMC's
  long-term plan is to move mcumgr back into `apache/mynewt-core`.
  (§1, §2, §3.4, §3.5)

**Build-time surface**
- **Q5.** Which command groups are default-on? → **On Mynewt there is no
  clear "default".** What is compiled in depends on which packages the
  application or a system component pulls in, and that is a per-sample
  configuration choice. The rule of thumb is that a group must be
  **explicitly enabled by the user** — either directly in the app, or
  transitively by pulling a package that depends on it (e.g. enabling USB
  may pull `img_mgmt`). Zephyr's defaults are out of model (see Q4).
  *(maintainer)*
- **Q6.** Where is the attacker-supplied chunk length validated relative
  to the stack copy? → **Handed to `cbor_read_object()` via `cbor_attr_t`**
  — i.e. the bound is enforced by the cborattr layer rather than by an
  explicit pre-copy check in the handler. *(maintainer)* The robustness of
  that path is still worth a scan's attention (§6).
- **Q7.** Does `fs_mgmt` constrain paths to an intended directory, or is
  full-filesystem read/write the intended (integrator-gated) behaviour?
  *(still open)*
- **Q8.** What do the `img_mgmt` "dummy header" / direct-upload syscfg
  toggles do? → **They are used in unit tests.** Not a production
  security surface. *(maintainer)*

**Parse hardening**
- **Q9.** How are reserved `nh_flags` bits and server-side receipt of
  `*_RSP` opcodes handled?
- **Q10.** Is `nh_len` ever used as a copy size before being validated
  against the actually-received byte count?
- **Q11.** Behaviour on unknown `(group,id)` — guaranteed clean error?
- **Q12.** Confirm `nh_seq` carries no security/ordering guarantee.
- **Q13.** Are `os_mgmt` diagnostics (taskstat/mpstat) intentional
  information disclosure to any transport peer? → **Yes.** *(maintainer)*
- **Q14.** Is `shell_mgmt` intended strictly for development? → **Believed
  so — but it is a Zephyr-only feature**, so for this model (Mynewt
  porting layer, Q4) it is largely moot. *(maintainer)*
  *Note (move into core):* on Mynewt the same remote shell execution
  group is provided by `sys/shell` when `SHELL_BRIDGE` is enabled (it is
  disabled by default), so the shell exec rows above refer to it.
- **Q15.** Does mcumgr bound CBOR nesting/size before handing the payload
  to tinycbor? → **No — that is up to the decoder.** Robustness against
  hostile CBOR is tinycbor's/cborattr's, which makes that path a
  first-order target for review (§6). *(maintainer)*
- **Q16.** Any intended DoS/rate-limit posture? → **It is on the
  transport.** mcumgr claims none of its own. *(maintainer)*
- **Q17.** Confirm no constant-time / side-channel guarantees are claimed.
  → **Confirmed.** *(maintainer)*

**Meta — ANSWERED**
- **Q18.** OK for this `THREAT_MODEL.md` to become the canonical model,
  reached via `AGENTS.md → SECURITY.md → THREAT_MODEL.md`? → **Yes** —
  that chain lands in this same PR.
- **Q19.** Should the model scope itself to the Apache repo only, or note
  Zephyr divergences? → **Apache repo only.** Zephyr forked mcumgr and no
  longer consumes this repository (§3.5). *(maintainer)*

**Still open** — Q7 (`fs_mgmt` path constraint) and the parse-hardening
questions Q9–Q12 above. Szymon noted some of these touch very low-level
detail; they are not blocking and the affected claims stay *(inferred)*.

## Appendix: existing security-policy artefacts → §x back-map

At `master @ 0b63bc54308b`, `apache/mynewt-mcumgr` contains **no
`SECURITY.md`, no `AGENTS.md`, and no prior threat-model document**
(only `protocol.md` and the per-transport `transport/*.md` notes, which
are protocol documentation, not security policy). There is therefore
nothing to back-map or supersede; this is a greenfield v0. The
`protocol.md` frame/format details are cited inline above as
*(documented)*.
