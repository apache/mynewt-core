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
- **Companion models:** `apache/mynewt-nimble` (the BLE stack — its own
  model covers the radio surface). Where a surface is fully modelled in a
  sibling document, this model points there rather than duplicating.
- **Management (mcumgr):** mcumgr, the SMP management library, was moved
  into `mynewt-core` (`mgmt/mcumgr/`) from the former
  `apache/mynewt-mcumgr` repository and is treated as any other core
  subsystem. Its threat model (drafted by the ASF Security team and
  reviewed by Szymon Janc, Mynewt PMC, on 2026-07-27) is merged into this
  document: §2, §5a, §6.1, §7–§13, and §14 Q19–Q33.
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
| **Management** | `mgmt/mcumgr` (SMP core: `mgmt`, `smp`, `omp`; command groups `cmd/{img,fs,os,log,stat}_mgmt`), Mynewt glue and transports `mgmt/smp` (`transport/{ble,smp_shell,smp_uart}`), `mgmt/oicmgr` (OMP over `net/oic`), `mgmt/{imgmgr,newtmgr,mgmt,image_header}`; other groups in `sys/config`, `sys/shell` (`SHELL_BRIDGE`), `test/{crash_test,runtest}` | **yes** | untrusted SMP/OMP frame parse + dispatch + CBOR decode, and command handlers with **very different** privilege footprints (firmware write vs. read a stat) (§6.1). No authn/authz by design (§9) |
| **Crypto** | `crypto/mbedtls` (**vendored upstream**) | as a library, by the code above | primitives; mbedtls security is upstream's (§3, §11a). tinycrypt was **removed** from core before the 1.15 releases *(maintainer)* |
| **Boot / image** | `boot/{split,split_app,startup,stub}`, `mgmt/image_header` | at boot, over the staged image | image *authenticity* gate is the signature-verifying bootloader (MCUboot); see §9/§10 |
| **Sys / local mgmt** | `sys/{console,shell,config,log,coredump,fault,reboot,mfg,flash_map,stats,id}` | `console`/`shell` = local serial surface; also reachable remotely via `SHELL_BRIDGE` over SMP | local management surface (§6, §11) |
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
  glue; `img_mgmt` only **stages** an uploaded image into the secondary
  slot. *(maintainer — §14 Q2, confirmed)*
- Not a secure management channel. mcumgr/SMP provides **no**
  authentication, authorization, confidentiality, integrity, or replay
  protection of its own (§9). *(maintainer — §14 Q19, confirmed)*
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
5. **The SMP client** (`mcumgr-cli`, `newtmgr`, mobile/desktop apps) —
   separate repos. A finding that requires a malicious *client* is in
   model only insofar as it is the device-side server mishandling a
   crafted frame (§6.1).
6. **The BLE radio surface** — covered in `apache/mynewt-nimble`'s model,
   including the link security (pairing/bonding/encryption) that may gate
   the BLE SMP transport. Link security of other management transports
   (serial physical access, IP reachability) is the integrator's (§10).
7. **Physical / invasive / side-channel and supply-chain** concerns.
8. **Zephyr's fork of mcumgr.** Zephyr maintains its own fork of mcumgr in
   the Zephyr tree; Zephyr-only features and divergences are out of model.
   *(maintainer — §14 Q20)*

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
to gate them (§10) + the sibling model for BLE (nimble).

For the management surface the trust transition is **SMP/OMP frame
ingress**: the point where a transport hands a frame up to the SMP core.
After that point there is no further internal boundary — a command handler
that the integrator compiled in can do whatever that command does (flash
write to the image slot, file read/write, reset, stats read, shell exec),
to whoever can send the frame. The security of management therefore rests
almost entirely on **who can put a frame on an enabled transport** (§7,
§10), not on anything SMP itself checks.

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
- **Which `mgmt/` transports and command groups are compiled in** (SMP
  over BLE / console / UART, OMP over `net/oic`; `img_mgmt`, `fs_mgmt`,
  `os_mgmt`, `log_mgmt`, `stat_mgmt`, config, shell exec via
  `SHELL_BRIDGE`). Remote shell execution and `fs_mgmt` (arbitrary file
  read/write) are the highest-power groups and should be off in most
  production builds. **There is no meaningful "default" set**: what is
  compiled in follows from which packages the application — or a system
  component — pulls in. A group must be **explicitly enabled by the
  user**, either directly in the app or transitively by pulling a package
  that depends on it (enabling USB, for instance, may pull `img_mgmt`).
  *(maintainer — §14 Q21)*
- **Management upload chunk buffers are stack-allocated.**
  `IMG_MGMT_UL_CHUNK_SIZE` and `FS_MGMT_UL_CHUNK_SIZE` default to **512**
  and a buffer of this size is allocated **on the stack** while handling
  an upload; the attacker-supplied chunk length must be bounded before the
  copy. The bound is enforced by `cbor_read_object()` via `cbor_attr_t`
  rather than by an explicit pre-copy check in the handler.
  *(documented: `mgmt/mcumgr/cmd/{img,fs}_mgmt/syscfg.yml`; maintainer —
  §14 Q22)*
- **`FS_MGMT_PATH_SIZE` (default 64)** bounds the file-path buffer for
  `fs_mgmt`; path handling against this bound, and any path-traversal
  containment, is a per-build concern. *(documented:
  `mgmt/mcumgr/cmd/fs_mgmt/syscfg.yml`; §14 Q23)*
- **SMP frame endianness.** SMP adds optional little-endian support on top
  of NMP's mandatory big-endian header; both decode paths are reachable.
  *(documented: `mgmt/mcumgr/docs/protocol.md`)*
- **`img_mgmt` "dummy header" / direct-upload toggles** are used in unit
  tests; not a production security surface. *(maintainer — §14 Q24)*
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
  surface when enabled. *(documented: `sys/shell` `SHELL_BRIDGE`)*
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
| SMP / newtmgr / OMP mgmt frames | `mgmt/*` | a peer on the mgmt transport | memory-safe frame parse, dispatch and CBOR decode (§6.1); no authn/authz by design |
| Console / shell input | `sys/console`, `sys/shell` | local serial (or remote via `SHELL_BRIDGE`) | bounded line handling; shell is powerful-by-design (§11) |
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
  network/mgmt surface. *(inferred — §14 Q14)* For management, mcumgr does
  not rate-limit inbound frames; backpressure is the transport's.
  *(maintainer — §14 Q32)*

### §6.1 Management (SMP) frames

If the attacker can reach an enabled management transport, **every byte
of the SMP frame is attacker-controlled** — header and payload alike. OMP
carries the same header (as a CBOR byte string) and payload over CoAP.

#### SMP frame header trust table (`struct mgmt_hdr`)

| Field | Meaning | Attacker-controllable? | Core/handler must enforce |
| --- | --- | --- | --- |
| `nh_op` (3 bits) | READ / WRITE / *_RSP | **yes** | reject/ignore response opcodes arriving at a server; route only valid ops *(inferred — §14 Q25)* |
| `nh_flags` | reserved | **yes** | defined-bit validation; ignore reserved bits safely *(documented: "TBD"; §14 Q25)* |
| `nh_len` | **claimed** payload length | **yes** | the claimed length must be validated against the *actually received* byte count and against buffer bounds before use — never trusted as the copy size *(inferred — §14 Q26)* |
| `nh_group` | command group selector | **yes** | dispatch only to a registered group; unknown group → clean error, not UB *(inferred — §14 Q27)* |
| `nh_seq` | sequence number | **yes** | no replay/ordering guarantee is claimed (§9); used only to correlate response *(documented: "TBD"; §14 Q28)* |
| `nh_id` | command within group | **yes** | dispatch only to a registered handler; unknown id → clean error *(inferred — §14 Q27)* |

#### CBOR payload trust table (per high-value command)

| Command group → command | Attacker-supplied fields | What the handler does with them | Must enforce |
| --- | --- | --- | --- |
| `img_mgmt` upload | image `data` chunk, `off`(set), `len`, `sha` | copies `data` into a **stack** buffer, writes it to the staging flash slot at `off` | chunk length bounded vs `IMG_MGMT_UL_CHUNK_SIZE` *before* copy (§14 Q22); `off` monotonic / bounded to slot size; **never** treat acceptance as "this image is trusted" (that is MCUboot, §9) *(documented + inferred)* |
| `fs_mgmt` upload/download | file `name`/path, `off`, `data`, `len` | opens a file at `name`, reads/writes at `off` | path bounded to `FS_MGMT_PATH_SIZE`; **path-traversal containment** to an intended directory (or the integrator accepts full-FS access); chunk bound as above *(inferred — §14 Q23)* |
| `os_mgmt` taskstat / mpstat / datetime / reset | command params | returns task/memory diagnostics; performs a device reset | bounded output encoding; reset only after the documented delay *(documented: `OS_MGMT_RESET_MS`)* — these **leak internal state** and **reboot the device** to anyone on the transport, by design *(maintainer — §14 Q29)* |
| shell exec (`sys/shell`, `SHELL_BRIDGE`) | a shell command line | executes it in the device shell | **this is arbitrary command execution by design** when enabled; intended for trusted/dev contexts only *(§14 Q21, Q30)* |

#### Size / shape / rate

- **CBOR depth/size.** The payload is decoded by `encoding/tinycbor` /
  `encoding/cborattr`. mcumgr does **not** bound CBOR nesting/size before
  decode — robustness against deeply-nested, truncated, or oversized CBOR
  is the decoder's, which makes that path a first-order review target.
  *(maintainer — §14 Q31; see also Q12)*

## §7 Adversary model

| Actor | In scope? | Capabilities |
| --- | --- | --- |
| **Network/radio peer reachable by an enabled `net/` stack** | **yes — primary remote** | craft arbitrary protocol frames (IP/CoAP/LoRa/MQTT/OSDP/Wi-Fi); fuzz; replay; flood. |
| **Peer on an enabled `mgmt/` transport** | **yes** | craft arbitrary SMP/OMP frames (any group/id/len, any CBOR), replay, flood, fuzz. For BLE: in radio range (bounded by whatever link security the integrator enabled in nimble — possibly none). For serial: physical/console access. For IP (OMP over CoAP): network reachability. No auth by design (§9). |
| **A malformed-but-deliverable mgmt frame from an otherwise "legitimate" client** (buggy or compromised management tool) | **yes — parser robustness must hold** | drives the parse/dispatch/decode path with adversarial bytes; in model for memory safety, hang, unbounded stack/heap use. |
| **The mgmt transport's link-layer peer** (e.g. a paired BLE central) | **conditionally** | if the integrator required BLE bonding+encryption, the adversary is reduced to a *bonded* peer; if not, it is anyone in range. The reduction is the integrator's doing, not Mynewt's (§10). |
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

### Management asymmetry

Because SMP performs **no authentication or authorization** (§9), a
management adversary's power is **entirely determined by transport
reachability and the set of enabled command groups** — not by anything
mcumgr checks. A device that exposes `img_mgmt` + `fs_mgmt` + shell exec
over an unencrypted, un-bonded BLE connection is, by design, **fully
controllable by anyone in radio range**. This is not a Mynewt
vulnerability; it is the integrator operating management outside its
intended trust assumptions (§10, §11). Conversely, a memory-safety bug in
the frame parser **is** Mynewt's, because it breaks even when the
transport is perfectly secured.

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
5. **Memory-safe parsing and dispatch of SMP frames on the compiled-in
   management command groups.** A special case of property 1, called out
   because it is the whole management attack surface. *(inferred — §14
   Q26/Q27)*
   - *Violation:* a crafted frame (bad `nh_len`, oversized upload chunk,
     malformed CBOR, unknown group/id) causes out-of-bounds read/write,
     stack overflow, or controlled corruption.
   - *Severity:* **high**.
6. **Bounded stack use during management uploads**, with chunk length
   validated against `*_UL_CHUNK_SIZE`. *(documented config; maintainer —
   §14 Q22)*
   - *Violation:* an upload chunk larger than the configured bound overruns
     the stack buffer.
   - *Severity:* **high**.
7. **Faithful implementation of the SMP wire format** (header layout,
   op/group/id semantics, CBOR encoding of responses). *(documented:
   `mgmt/mcumgr/docs/protocol.md`)*
   - *Violation:* a response that misencodes lengths/IDs such that a
     conforming client mis-parses it.
   - *Severity:* **low–medium** (interop / client-side, mostly).
8. **Clean rejection of unknown management groups/commands** (no dispatch
   into unregistered handlers). *(inferred — §14 Q27)*
   - *Violation:* an unknown `(group,id)` reaches uninitialised function
     state.
   - *Severity:* **medium–high**.

Management makes no claim of graceful behaviour under *resource
exhaustion* (flooding) or about *malicious but well-formed* commands
(those are §9/§10, by design).

## §9 Security properties the project does *not* provide

- **No process / memory isolation between firmware components.** *(inferred
  — §14 Q1)*
- **No authentication or authorization at the management layer**
  (`mgmt/`). SMP carries no credential and the server does not verify
  *who* sent a frame; there is no per-command permission model — if a
  group is compiled in, every command in it is available to every peer
  that can reach the transport. The transport and bootloader are the
  gates. *(maintainer — §14 Q19)*
- **No confidentiality, integrity, anti-tamper or replay protection at
  the SMP layer.** Frames are plaintext (secrecy is the transport's, e.g.
  BLE link encryption); `nh_seq` correlates responses, it is not a nonce.
  *(maintainer — §14 Q19, Q28)*
- **No image authenticity guarantee** — that is the signature-verifying
  bootloader's (MCUboot). Mynewt stages bytes. *(inferred — §14 Q2)*
- **No guarantee for vendored-mbedtls internal correctness** — upstream's
  (§3.1, §11a).
- **No availability/DoS guarantee** against a peer who can reach a
  network/mgmt surface. *(inferred — §14 Q14)* A management peer can
  flood, reset (`os_mgmt`), or wedge the device. *(maintainer — §14 Q32)*
- **No protection of secrets against a local operator with shell/console
  or coredump access**, when those are enabled. *(inferred — §14 Q5)*
- **No constant-time / side-channel guarantee** beyond what the chosen
  crypto backend provides; management claims none. *(maintainer — §14
  Q33)*

### False friends

- **`boot/` is not secure boot by itself** — the signature check is the
  bootloader's; `boot/` here is split-image/startup glue.
- **A `net/` stack accepting a connection is not authorization** — the
  service the app exposed is the app's policy choice.
- **"SMP" / "management protocol" sounds authenticated; it is not** — it
  is a transport-agnostic RPC with no security layer of its own.
- **`img_mgmt` image hash (`sha`) is an integrity/identification aid, not
  an authenticity check.** The cryptographic authenticity gate is
  MCUboot's signature verification, separately.

## §10 Downstream responsibilities

1. **Enable a signature-verifying bootloader (MCUboot)** and keep image
   verification on, so a staged image cannot execute unless signed.
2. **Expose only the `net/` stacks and `mgmt/` transports the product
   needs**, and gate each (link encryption, network segmentation, BLE
   bonding via nimble). For management: require BLE bonding + LE Secure
   Connections encryption (configured in nimble), restrict serial to
   physically-trusted access, firewall/authenticate any IP transport. An
   open management transport = full device control to anyone who can
   reach it.
3. **Keep `sys/shell` / `SHELL_BRIDGE` / `coredump` out of production**
   unless required and access-gated, and **compile in only the management
   groups you need** — treat shell exec and `fs_mgmt` as debug-only.
4. **Track upstream advisories for every vendored component** — not just
   mbedtls, but each package pulled in through a `repository.<name>`
   stanza (`lwip`, `littlefs`, `nanopb`, `wiznet`, `osdp`, vendor SDKs
   and drivers …) — and update the vendored copies. *(maintainer)*
5. **Provide a CSPRNG-quality RNG** for any crypto/mgmt use. Core exposes
   **no unified random API** — you are choosing between libc's and a
   platform TRNG driver (`hw/drivers/trng/`), so the quality of what your
   crypto consumes is your decision. *(maintainer — §14 Q4)*
6. **Size mempool/mbuf pools** so an input flood degrades gracefully, and
   bound/rate-limit at the transport if availability matters.
7. **Assume any reachable, enabled management command is fully exercised
   by an adversary**, and threat-model the *product* on that basis.

## §11 Known misuse patterns

- Shipping with `SHELL_BRIDGE`/`sys/shell` reachable over an open transport.
- Exposing `img_mgmt`/`fs_mgmt`/shell exec over an **unencrypted,
  un-bonded BLE** connection in a shipped product.
- **Relying on SMP for access control** ("only our app speaks SMP") — any
  peer on the transport can speak SMP.
- **Assuming firmware upload needs a credential** — it does not; only
  MCUboot's signature check stands between an uploaded image and boot,
  and only if enabled.
- Treating the image `sha` in an upload as an authenticity guarantee.
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
| "SMP / `mgmt/` endpoint has no authentication / authorization" | By design — authn/authz is not a management property; the integrator gates the transport. | §9, §10.2 |
| "Unauthenticated firmware update / DFU over BLE/serial" | Intended; image *execution* is gated by MCUboot signature verification, which is separate. | §9, §10.1 |
| "shell exec (`SHELL_BRIDGE`) allows arbitrary command execution" | Opt-in (disabled by default), build-time-gated debug feature for trusted contexts. | §5a, §6.1, §10.3 |
| "`os_mgmt` taskstat/mpstat leaks internal memory/task layout" | Diagnostic by design; same transport-trust assumption as every other command. | §6.1, §9 |
| "`os_mgmt` reset lets a peer reboot the device (DoS)" | No availability guarantee against an on-transport adversary is claimed. | §9 |
| "No replay protection — SMP frames can be replayed" | Correct; `nh_seq` is a correlator, not a nonce; not claimed. | §9 |
| "SMP frames are sent in cleartext" | Confidentiality is the transport's job (e.g. BLE link encryption), not SMP's. | §9, §10.2 |
| "Image `sha` is not a real signature" | Correct — it is an identifier/integrity aid; authenticity is MCUboot's. | §9 false-friends |
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
- Adding an authentication or authorization layer to SMP (would create
  real §8 properties and move several §9 items).
- A management transport whose default reachability differs (e.g. an
  always-on IP transport), or making a currently-opt-in management group
  (shell exec, `fs_mgmt`) default-on.
- A change to where management upload buffers live (stack → heap) or how
  chunk bounds are enforced.

## §13 Triage dispositions

| Disposition | Use when |
| --- | --- |
| **VALID** | Memory-unsafety / bounds / unbounded-resource / concurrency failure in any externally-reachable parser (`net/`, `mgmt/`, `fs/`, `encoding/`, image header) or kernel object reachable from input — for management, reachable by a crafted SMP/OMP frame on a compiled-in handler, i.e. it breaks *even with the transport perfectly secured*; a Mynewt-level crypto misconfiguration. |
| **OUT-OF-MODEL** | Depends on the absence of mgmt/network authn/authz/encryption/replay protection, on a protocol-spec weakness, on an internal flaw in **any vendored component** (§3.1), or on a malicious-but-well-formed management command. |
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
  scope** (§3.3).

**Meta — ANSWERED**
- **Q17.** OK for this `THREAT_MODEL.md` to be the canonical model,
  reached via `AGENTS.md → SECURITY.md → THREAT_MODEL.md`?
  → **Yes** — that chain lands in this same PR.
- **Q18.** Single umbrella model, or per-`net/`-stack sub-models?
  → **"For now lets do single umbrella."** Revisit per §12 if a
  particular stack warrants its own model later.

**Management (mcumgr / SMP)** — from the former `apache/mynewt-mcumgr`
model (its Q1–Q19); answered by Szymon Janc (Mynewt PMC) on 2026-07-27.
mcumgr's Q2 (MCUboot is the sole execution gate) and Q3 (single address
space) are the same as Q2 and Q1 above.
- **Q19.** SMP provides no authentication, authorization,
  confidentiality, integrity or replay protection of its own; all
  delegated to the transport/integrator. → **Confirmed.**
- **Q20.** Scope of the mcumgr model. → **Mynewt only.** Zephyr forked
  mcumgr into its own tree and no longer used the `apache/mynewt-mcumgr`
  repository; the PMC planned to move mcumgr back into `mynewt-core`,
  which has since been done (`mgmt/mcumgr/`, OS porting layer removed).
- **Q21.** Which command groups are default-on? → **On Mynewt there is no
  clear "default".** What is compiled in depends on which packages the
  application or a system component pulls in; a group must be
  **explicitly enabled by the user**, directly or transitively (e.g.
  enabling USB may pull `img_mgmt`).
- **Q22.** Where is the attacker-supplied upload chunk length validated
  relative to the stack copy? → **Handed to `cbor_read_object()` via
  `cbor_attr_t`** — the bound is enforced by the cborattr layer rather than
  by an explicit pre-copy check in the handler. The robustness of that
  path is still worth a scan's attention (§6.1).
- **Q23.** Does `fs_mgmt` constrain paths to an intended directory, or is
  full-filesystem read/write the intended (integrator-gated) behaviour?
  *(still open)*
- **Q24.** What do the `img_mgmt` "dummy header" / direct-upload syscfg
  toggles do? → **They are used in unit tests.** Not a production
  security surface.
- **Q25.** How are reserved `nh_flags` bits and server-side receipt of
  `*_RSP` opcodes handled? → **Request `nh_flags` are ignored.** SMP
  responses always carry `nh_flags = 0`; OMP echoes the request header
  (including flags) back. Only `MGMT_OP_READ` and `MGMT_OP_WRITE` are
  dispatched; any other opcode (including `*_RSP`) fails with
  `MGMT_ERR_EINVAL` in SMP (an error response is sent) and is rejected
  without calling a handler in OMP. *(code review of
  `mgmt/mcumgr/smp/src/smp.c`, `mgmt/mcumgr/omp/src/omp.c` — pending
  maintainer confirmation)*
- **Q26.** Is `nh_len` ever used as a copy size before being validated
  against the actually-received byte count? → **No.** In SMP the CBOR
  reader is bounded by the actually received data (`message_size` is the
  remaining mbuf packet length) and `nh_len` is only used, after the
  request was processed, to trim it from the packet with
  `os_mbuf_adj()`, which never trims more than the packet holds. Response
  `nh_len` is computed from the encoded bytes. Note that the payload
  decode is not limited to `nh_len`, so a request whose `nh_len` is
  shorter than its CBOR payload is decoded past `nh_len` (still within the
  received packet). In OMP the header is a CBOR byte string whose length
  must equal `sizeof(struct mgmt_hdr)` and `nh_len` is not used.
  *(code review — pending maintainer confirmation)*
- **Q27.** Behaviour on unknown `(group,id)` — guaranteed clean error?
  → **Yes.** `mgmt_find_handler()` returns `NULL` for an unregistered
  group, for a command id at or beyond the group's handler count, and for
  a command with neither read nor write handler; SMP then returns
  `MGMT_ERR_ENOTSUP` and OMP `MGMT_ERR_ENOENT`. A registered command
  without a handler for the requested op returns `MGMT_ERR_ENOTSUP`. Note
  that when several groups share a group id, lookup stops at the first
  such group if the command id is beyond its handler count. *(code
  review — pending maintainer confirmation)*
- **Q28.** Confirm `nh_seq` carries no security/ordering guarantee.
  → **Confirmed by code.** `nh_seq` is only copied from request to
  response header; it is not checked or tracked. *(code review — pending
  maintainer confirmation)*
- **Q29.** Are `os_mgmt` diagnostics (taskstat/mpstat) intentional
  information disclosure to any transport peer? → **Yes.**
- **Q30.** Is remote shell execution intended strictly for development?
  → **Believed so.** The answer was given for mcumgr's Zephyr-only
  `shell_mgmt`; on Mynewt the same capability is provided by `sys/shell`
  when `SHELL_BRIDGE` is enabled (disabled by default).
- **Q31.** Does mcumgr bound CBOR nesting/size before handing the payload
  to tinycbor? → **No — that is up to the decoder.** Robustness against
  hostile CBOR is tinycbor's/cborattr's (Q12).
- **Q32.** Any intended DoS/rate-limit posture for management? → **It is
  on the transport.** mcumgr claims none of its own.
- **Q33.** Confirm no constant-time / side-channel guarantees are claimed
  by management. → **Confirmed.**

**Still open** — the per-subsystem parser-robustness questions (Q6–Q13),
the kernel/resource questions (Q14, Q15) and the management question Q23
above. Q25–Q28 are answered from code review and await maintainer
confirmation. Szymon noted some
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

The management (mcumgr) content was originally a separate model in the
`apache/mynewt-mcumgr` repository (`master @ 0b63bc54308b`), drafted from
that repository's README, `protocol.md`, `transport/*.md` and
`cmd/*/syscfg.yml`, and reviewed by Szymon Janc on 2026-07-27. When
mcumgr was moved into `mynewt-core` (`mgmt/mcumgr/`) it was merged into
this document; paths were updated and its questions renumbered as
Q19–Q33.
