# Matter 1.4 + TinyC — Roadmap to the Final Goal

> Builds on `PLAN.md` (the phase tracker). That doc took us from nothing to a
> live relay toggle. This doc is the higher-level roadmap from that proof to
> a **certifiable Matter 1.4 device whose application behavior is defined in
> TinyC scripts** — the equivalent of Tasmota Berry Matter, but pure-C and
> driven by the TinyC VM.

---

## 0. Where we are (foundation — DONE, live on the C6)

```
✅ Crypto      SPAKE2+, AES-CCM, ECDH, ECDSA(RFC6979), HKDF/HMAC/SHA, CASE schedule
✅ Wire        TLV, message+protocol frame, MRP, secured (CCM) frame, PASE/CASE msgs
✅ Transport   on-network mDNS (_matterc._udp), UDP:5540 listener, udp_send
✅ Handshake   full PASE (SPAKE2+ mutual auth) — verified by independent prover
✅ Secured IM  encrypted InvokeRequest/Response both directions
✅ App         General Commissioning + OnOff cluster -> Tasmota relay toggle
✅ Tooling     host self-tests, pase_prover.py, safeboot-aware flash_c6.sh
```

The hard cryptographic + transport unknowns are retired. Everything below is
**bounded engineering**: breadth (clusters, data model), one remaining hard
codec (operational certificates), and the TinyC scripting layer.

---

## 1. Definition of "done"

A Tasmota build (`-DTINYC_MATTER`) that:
1. **Commissions with real controllers** — chip-tool, Apple Home, Google
   Home, Alexa, Home Assistant (matter-server) — over on-network IP, via the
   standard sequence (Attestation → CSR → AddNOC → CASE).
2. Implements the **Matter 1.4 mandatory data model** (root-node clusters,
   Read/Write/Subscribe, ACL) and a set of **device types** (plug, light,
   sensors, and — the flagship — **energy measurement**).
3. Lets a **TinyC script define the Matter device**: declare endpoints +
   clusters, publish attribute values, and handle commands — no firmware
   rebuild. The C core stays generic; the script is the "application".
4. Fits the flash/RAM budget alongside Tasmota + the TinyC VM (the whole
   reason for pure-C over Berry's ~343 KB).

Cert (formal CSA) is optional/stretch; **ecosystem interop is the practical
bar.** Dev VID 0xFFF1 + chosen PID for testing.

---

## 2. The gap (beyond the current relay-toggle proof)

| Area | Have | Need for 1.4 |
|---|---|---|
| Commissioning | PASE + GC + OnOff over PASE | Attestation, CSR, **operational-cert codec**, AddNOC, CASE, fabric store |
| Sessions | one PASE session | CASE operational sessions, multi-fabric, multi-controller, counter persistence |
| Interaction Model | Invoke only | **Read, Write, Subscribe** (+ report engine), wildcard paths |
| Security model | session keys | **Access Control List** (0x001F) enforcement |
| Clusters | GC(0x0030), OnOff(0x0006) | the mandatory root-node set + app clusters |
| Data model | hardcoded | endpoint/cluster/attribute registry (script-driven) |
| Scripting | none | TinyC Matter API + callbacks |

---

## 3. Phased plan

### Phase A — Standard commissioning + CASE  (real controller pairs)
Goal: `chip-tool pairing onnetwork <id> 20202021` completes; device reachable
on an operational CASE session.
- **A1 Operational certificate codec** (`mtrc_cert`): Matter compact-TLV
  cert (RCAC/ICAC/NOC).
  - ✅ **A1a parse + field extraction** — DONE (host-tested). Extracts
    subject node-id/fabric-id, issuer ids, public key, signature, is-CA
    from NOC + RCAC TLV. `test/build_cert.sh`.
  - ⏭ **A1b chain-signature verify** — reconstruct the X.509 DER TBS and
    ECDSA-verify NOC←ICAC←RCAC (or relaxed/trust-on-store for first interop).
- **A2 Device Attestation**: DAC/PAI + Certification Declaration (use the CSA
  test PAA/PAI/DAC set), `AttestationRequest`/`CertificateChainRequest`
  signing, attestation nonce/TBS.
- **A3 Node Operational Credentials cluster (0x003E)**:
  - ✅ **A3a PKCS#10 CSR builder** (`mtrc_csr`) — host-tested (build_csr.sh).
  - ✅ **A3b CSRRequest handler** — DONE (device-verified on C6). Parses the
    InvokeRequest CommandFields (CSRNonce), generates a fresh operational
    keypair, builds the CSR, returns CSRResponse{NOCSRElements, attestation
    signature} over the PASE session. Verified: prover sends CSRRequest →
    device returns a valid 189-byte PKCS#10 CSR for its new operational key.
    (attestationSignature uses a placeholder DAC until A2.)
  - ✅ **A3c** `AddTrustedRootCertificate` + `AddNOC` — DONE (device-verified).
    AddTrustedRootCertificate parses the RCAC (mtrc_cert) → stores the root
    pubkey; AddNOC parses the NOC → installs a fabric into `mtrc_store` (root
    pubkey + fabric/node ids from the NOC + the CSR-generated operational key
    + the IPK), returns NOCResponse{statusCode, fabricIndex, debugText}.
    Verified: prover drives PASE → CSRRequest → AddTrustedRootCertificate →
    AddNOC → NOCResponse statusCode=0, fabricIndex=2 (fabric installed).
  - ⏭ CASE against the *installed* fabric (end-to-end PASE→commission→CASE);
    `UpdateNOC`, `RemoveFabric`; persist the fabric table via the port kv.
- **A4 Fabric store** (`mtrc_store`):
  - ✅ **Fabric table + serializer** — DONE (host-tested). Fixed-capacity
    in-memory table (root pubkey, fabric/node id, IPK, operational keypair,
    NOC/ICAC) with alloc/index-assign/lookup/remove/enumerate + a flat
    serialize/deserialize blob for kv persistence. `test/build_store.sh`
    (30 checks: alloc, index reuse, lookups, round-trip, malformed-reject).
  - ⏭ Wire into matter_c: load on init / save on AddNOC+RemoveFabric via the
    port kv (UFS); operational message-counter persistence across reboot.
- **A5 CASE responder**:
  - ✅ **Full handshake proven host-side** — `test/build_case_full.sh` runs an
    initiator AND responder against each other in C (mtrc_case schedule +
    mtrc_case_msg codecs + mtrc_crypto): destinationId match → ECDH → S2K →
    Sigma2 (TBE seal + TBS sign) → S3K → Sigma3 → mutual ECDSA verify → both
    sides derive identical I2R/R2I/Att; an operational message round-trips
    (26 checks). The protocol/key-schedule risk is retired.
  - ✅ **Sigma1 → Sigma2 on the device** — DONE (device-verified on C6). New
    SC opcodes (Sigma1=0x30/2=0x31/3=0x32); `case_handle_sigma1` matches the
    initiator's destinationId to a stored fabric (`mtrc_store`), does ECDH,
    derives S2K, signs TBSData2 with the operational key, seals TBEData2, sends
    Sigma2. A fixed test fabric is pre-provisioned behind `-DMTRC_CASE_TEST_FABRIC`
    (off by default — no creds ship). The prover, extended into a CASE
    initiator, sends Sigma1 and confirms: S2K agreed, TBE2 opened, device
    signature valid → responder authenticated.
  - ✅ **Sigma3 → operational session** (A5.2b) — DONE (device-verified on C6).
    `case_handle_sigma3` decrypts TBEData3, parses the initiator NOC (A1a) for
    its pubkey, verifies the Sigma3 signature, derives I2R/R2I/Att, sends a
    success StatusReport. A small TX-context refactor (`tx_use_pase/case`) lets
    the existing IM handlers answer over either the PASE or the CASE session.
    Verified: prover drives Sigma1/2/3 → mutual auth → an encrypted IM
    ReadRequest over the CASE session returns ReportData. **A mutually-
    authenticated operational session + IM-over-CASE now works on hardware.**
  - ⏭ Harden: peer-NOC chain verify (A1b, currently relaxed/A1a-pubkey-only),
    operational mDNS advert (`_matter._tcp`), CASE message-counter persistence.
- **Exit**: chip-tool pairs and reads Basic Information over CASE.

### Phase B — Core data model (Read/Write/Subscribe + mandatory clusters)
Goal: controllers can browse + monitor the node like a real Matter device.
- **B1 IM Read** (`ReadRequest`/`ReportData`) — ✅ STARTED, live on C6. Parses
  AttributePathIB, reports the value via the new `on_attr_read` port (OnOff
  0x0006/0 returns the **live relay state**; BasicInfo VID/PID). Verified:
  set relay ON/OFF (Tasmota) -> Matter Read reports OnOff=1/0. Remaining:
  multi-attribute reports, wildcard paths, a real attribute registry.
- **B2 IM Write** (`WriteRequest`/`WriteResponse`) — pending (needs a
  writable attribute / cluster first).
- **B3 Subscriptions** — ✅ STARTED, live on C6. SubscribeRequest -> priming
  ReportData (with SubscriptionId) + SubscribeResponse; the report engine in
  matter_loop sends periodic (maxInterval) + change-driven ReportData.
  Verified: prover subscribes to OnOff -> priming + response + periodic
  report received. Remaining: multi-attribute subscriptions, the
  StatusResponse handshake leg, keep-alive/resubscribe, multiple subs.
- **B4 Access Control cluster (0x001F)** + ACL enforcement on every IM op.
- **B5 Root-node mandatory clusters** on endpoint 0:
  - ✅ **Descriptor (0x001D)** — DONE (device-verified). Built from the
    registry on every endpoint: DeviceTypeList, ServerList (the endpoint's
    clusters), ClientList (empty), PartsList (ep0 → child endpoints). New
    `mtrc_im` list-report builders (array-of-uint + DeviceTypeStruct);
    `test/build_im_list.sh` round-trips the TLV. Lets a controller browse the
    node. Remaining: global attributes (AttributeList 0xFFFB, etc.) +
    Descriptor over Subscribe.
  - ⏭ Basic Information (0x0028, more attrs), General Commissioning (0x0030,
    full), Network Commissioning (0x0031), General Diagnostics (0x0033),
    Administrator Commissioning (0x003C), Operational Credentials (0x003E),
    Group Key Management (0x003F).
- **Exit**: passes the IM/data-model portions of the chip cert tests.

### Phase C — TinyC Matter scripting API  (the "with TinyC" goal)
Goal: a `.tc` script defines the Matter device; the C core is generic.
Mirrors the retired HomeKit pattern (`hkAdd`/`hkStart`/`HomeKitWrite`).
- **C1 Data-model registry** (`mtrc_dm`) — ✅ DONE (host-tested). Dynamic
  endpoint/cluster/attribute tables (fixed-capacity, no malloc) replace the
  hardcoded `attr_value()` if/else ladder. `matter_init` seeds root (BasicInfo
  VID/PID) + a default OnOff plug endpoint; `matter_add_endpoint` attaches a
  device-type's cluster set; `matter_set_attr` decodes a TLV scalar into the
  registry; IM Read/Subscribe resolve values registry-first (live host
  `on_attr_read` still wins). `test/build_dm.sh` (declare/get/set + change
  detection, idempotency, capacity, enumeration). The B-phase clusters and the
  Phase-C TinyC syscalls now build on this registry instead of hardcoding.
- **C2 TinyC syscalls** (new `mtr*` builtins, append-only JMPTBL):
  - ✅ **C2a data-model syscalls** — DONE. SYS_MTR_ADD/CLUSTER/ATTR/SET/GET/
    START/RESET (398–404, append-only). Compiler: `matterAdd`/`matterCluster`/
    `matterAttr`/`matterSet`/`matterGet`/`matterStart`/`matterReset` builtins +
    predefined constants (`MATTER_PLUG`/…, `CLUSTER_ONOFF`/…, `MTR_U32`/…). VM
    handlers call the matter_c scripting API (`matter_add_endpoint`,
    `matter_set_attr_uint`, …) — guarded by `USE_MATTER_C`, with `#else` stubs.
    `examples/matter_plug.tc` (plug + Electrical Power Measurement); IDE
    simulator stubs in `vm.js`; bundle rebuilt; TINYC_RELEASE 1.3.21.
    - `matterAdd(deviceType)` → endpoint id
    - `matterCluster(ep, clusterId)` / `matterAttr(ep, cl, attrId, type)`
    - `matterSet(ep, cl, attr, value)` → updates attr (subs fire next loop)
    - `matterGet(ep, cl, attr)` ; `matterStart()` ; `matterReset()`
  - ✅ **C2b `MatterInvoke(ep, cl, cmd)` callback** — DONE. New `on_command`
    port: the core routes an Invoke to the script's `MatterInvoke` (every slot
    that defines it, like `HomeKitWrite`); when no script handles it, the
    built-in OnOff→relay default applies — so a script that drives the relay
    itself never double-toggles. Other app clusters return SUCCESS only if a
    script handled them, else UNSUPPORTED_COMMAND. `examples/matter_plug.tc`
    now owns OnOff via `MatterInvoke` (drives relay 1 through `tasm_power`).
  - ⏭ Command-field arguments (e.g. Level MoveToLevel level) — `MatterInvoke`
    currently passes (ep, cluster, cmd); parsing CommandFields into `arg` is
    the next refinement.
  - ⏭ `matterEvent(ep, cl, eventId, data)` to emit events.
- **C3 xdrv glue**: route the data-model registry's command/attr callbacks
  to the VM; the OnOff→relay path becomes a script (`MatterInvoke` →
  `power(...)`), not hardcoded C.
- **C4 IDE + examples**: a TinyC example per device type; bundle.py builtins;
  Reference docs (EN/DE).
- **Exit**: a `.tc` script defines a working plug + a sensor, commissioned
  and controlled from Apple Home — zero firmware rebuild to change the device.

### Phase D — Device types + Matter 1.4 application clusters
Goal: useful products, with Tasmota's strengths front and center.
- **D1 Lighting/control**: Level Control (0x0008), Color Control (0x0300);
  device types OnOff Plug (0x010A), Dimmable/Color Light (0x0101/0x010D).
- **D2 Sensors**: Temperature (0x0402), Humidity (0x0405), Pressure,
  Illuminance, Occupancy, Boolean State (contact/leak) — bridged from
  Tasmota sensors (the dual-driver/SML data already in the firmware).
- **D3 ⭐ Energy (Matter 1.4 flagship for Tasmota)**: Electrical Power
  Measurement (0x0090) + Electrical Energy Measurement (0x0091) — expose
  Tasmota's SML / energy-meter data as a native Matter electrical sensor.
  Optionally Device Energy Management / Water Heater for surplus control.
  *This is the differentiator: a scriptable Matter energy device from the
  firmware that already does the metering.*
- **D4 Bridge / Aggregator** (0x000E): expose multiple Tasmota relays/
  sensors as one Matter bridge node (like Berry Matter's bridge).
- **Exit**: the device types interoperate in Apple/Google/Alexa/HA.

### Phase E — Interop, robustness, persistence, (optional) cert
- Multi-fabric / multi-admin / multiple simultaneous CASE sessions.
- Full MRP (retransmit wired into matter_loop; the timer exists), message-
  counter persistence, session resumption.
- Privacy (§4.8) for strict compliance.
- Soak + ecosystem matrix (Apple/Google/Alexa/HA) test passes.
- Flash/RAM budget pass on 4M alongside TinyC; feature gates for trims.
- (Stretch) CSA cert path: VID/PID, Test Harness, PAA.

---

## 4. TinyC integration — design sketch (the distinctive value)

The C core (`matter_c`) becomes a **generic Matter engine**; the **TinyC
script is the application**, exactly mirroring how the fork already lets a
`.tc` script be a Shelly/EcoTracker/HomeKit device. Example target script:

```c
// a Matter smart plug + power sensor, defined entirely in TinyC
int ep;
void main() {
  ep = matterAdd(DEVTYPE_ONOFF_PLUGIN);     // endpoint with OnOff + needed clusters
  matterCluster(ep, CLUSTER_ELEC_POWER);    // add Electrical Power Measurement
  matterStart();                            // advertise + accept commissioning
}
void EverySecond() {
  matterSet(ep, CLUSTER_ELEC_POWER, ATTR_ACTIVE_POWER, smlGet("Power"));  // live W
}
void MatterInvoke(int e, int cluster, int cmd) {   // controller sent a command
  if (cluster == CLUSTER_ONOFF) power(1, cmd);      // 0/1/2 -> relay
}
```

- **Reuses the HomeKit slot**: same gate (`TINYC_MATTER` replaces
  `TINYC_HOMEKIT`), same callback shape (`MatterInvoke` ~ `HomeKitWrite`).
- **Append-only JMPTBL** for the new `mtr*` syscalls (per the project's hard
  rule) — never changes existing entries.
- The data-model registry + subscription engine live in C; the script just
  declares structure + pushes values + handles commands. Heavy crypto/IM
  never touches the VM.

---

## 5. Matter 1.4 specifics worth targeting

1.4 is a good fit for Tasmota; the highest-value 1.4 items here:
- **Energy clusters** (Electrical Power/Energy Measurement) — Tasmota already
  has the metering; this is the flagship.
- **Enhanced multi-admin** robustness.
- **Latching switches / Generic Switch** for button devices.
- (Lower priority for mains Tasmota: ICD/sleepy, Thread — we're Wi-Fi/IP.)

---

## 6. Risks, effort, sequencing

| Phase | Risk | Effort (part-time) |
|---|---|---|
| A commissioning + CASE | med — operational-cert codec is fiddly; attestation cert set | 4–6 wk |
| B data model + subs + clusters | med — subscriptions/report engine is the big one | 6–10 wk |
| C TinyC API | low — pattern proven (HomeKit); registry design is the work | 3–4 wk |
| D device types + energy | low–med — breadth; energy mapping from SML | 4–8 wk |
| E interop/robustness/cert | open-ended | ongoing |

Rough total to "1.4 + TinyC, ecosystem-interoperable": **~6–9 months
part-time**. No remaining cryptographic unknowns; risk is now schedule/breadth.

**Sequencing note**: A before C (need real CASE sessions to drive a
script-defined device from a real controller), but C1 (the registry) can be
designed in parallel with B so B's clusters are built on the registry from
the start (avoids a rewrite). D rides on C.

---

## 7. Immediate next step

**Phase A1 — the operational-certificate codec** (`mtrc_cert`): the one
remaining hard piece. Capture chip-tool's real RCAC/NOC during a pairing
attempt (the device already gets that far over PASE) and validate the codec
against them. Once certs parse + chain-verify, AddNOC + CASE fall into place
and we get a *standard* chip-tool pairing — after which everything is data
model + clusters + the TinyC API.
