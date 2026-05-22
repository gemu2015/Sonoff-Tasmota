# Matter-C Port — Implementation Plan

> **Status:** scaffolded. Library skeleton + host gate are in place and
> build clean on both paths; protocol layers are stubs (return
> NOT_IMPLEMENTED). This document is the spec for a from-scratch, pure-C
> Matter implementation for the gemu2015/Sonoff-Tasmota fork, **inspired
> by** (not converted from) the device-subset architecture of Tasmota
> Berry Matter (S. Hadinger). GPLv3.
>
> **Location:** `lib/libesp32_div/matter_c/` (alongside `homekit/`, so the
> existing tinyc `lib_extra_dirs` discover it). Portable to other firmware
> by copying the folder — `library.json` + the firmware-agnostic
> `matter_c.h` port/HAL are the reuse contract.
>
> **Host gate (mutually exclusive with HomeKit):** in the fork's local
> `user_config_override.h` — `#undef USE_HOMEKIT/USE_MATTER_C` then
> `#if defined(TINYC_MATTER) → USE_MATTER_C #elif defined(TINYC_HOMEKIT) →
> USE_HOMEKIT`. Local build envs `tinyc32c3-matter` / `tinyc32s3-matter`
> add `-DTINYC_MATTER` + unflag `-DTINYC_HOMEKIT`. The xdrv exposes a `/mt`
> pairing page (twin of HomeKit's `/hk`) under `#ifdef USE_MATTER_C`.
> Both paths build: c3-matter 83.9% flash, c3-homekit 92.3% (unchanged).

---

## 0. Provenance discipline — what earns "inspired by, not converted from"

A constraint on *method*, not just a label:

| Allowed (→ "inspired by") | Forbidden (→ "converted from") |
|---|---|
| Implement wire format / protocol from the **CSA Matter 1.4.1 spec** (compacted copies live in `lib/libesp32/berry_matter/specs_for_ai/`) | Opening any `Matter_*.be` and transcribing a function body |
| Adopt the **architectural pattern** (layered device-plugin inheritance, session store, IM dispatch) as a general design idea | Copying class / method / field names or file structure 1:1 |
| Use Berry Matter to decide **which device types** are worth shipping | Mirroring Berry's exact inheritance numbering / hierarchy |
| Re-derive pure algorithms (Verhoeff, Base38, QR, SPAKE2+ assembly) from the spec | Lifting the Berry algorithm implementation verbatim |
| Call the **BearSSL `br_*` crypto API** directly (BSD library) | Copying Berry Matter's crypto *glue* — write SPAKE2+/CASE assembly from the spec |

**Distinct identity:** namespace `mtrc_`, files `xdrv_NN_matter_c.ino` + this
`lib/matter_c/` dir, C-native idioms (tagged-dispatch structs + vtables,
not transpiled Berry classes). Different names, different layout, spec-sourced
logic.

**Courtesy:** notify Hadinger before starting. Attribution in headers:
"inspired by the device-subset architecture of Tasmota Berry Matter
(S. Hadinger); crypto via BearSSL (T. Pornin, BSD)."

---

## 1. Tech-stack decisions

| Concern | Choice | Why |
|---|---|---|
| **Crypto** | **BearSSL** — `lib/lib_ssl/bearssl-esp8266` (RAM-tuned, used on ESP32 too) | Hadinger picked it for Matter precisely because **mbedTLS RAM footprint is too heavy on ESP32**. BearSSL `i15`/`m15` P-256 uses **15-bit-limb, stack-only bignums (no malloc)** → bounded, small RAM under WiFi+web+VM heap pressure. Already linked in any TLS build ⇒ **~0 incremental crypto flash**. |
| **Crypto primitives present** | `ec_p256_m15.c` + `ec_prime_i15.c` (P-256, incl. `br_ec` **`muladd`** = `A·x+B·y`, exactly SPAKE2+'s `X=x·P+w0·M`), `ecdsa_i15_*_raw` (raw ECDSA, Matter uses raw not ASN.1), `aead/` AES-CCM, `hash/` SHA-256, `kdf/` HKDF | All Matter crypto needs are covered by the resident library |
| **Crypto glue we write** | SPAKE2+ assembly (M/N points, w0/w1 derivation, X/Y/Z/V, transcript TT), CASE session derivation, HKDF labels | From the spec — keeps provenance clean |
| **Commissioning transport** | BLE via **NimBLE** (already in Tasmota), Matter **BTP** over GATT | No WiFi creds needed pre-commission |
| **Operational transport** | UDP over **lwIP** (IPv6-first, IPv4 fallback) | Standard Matter operational path |
| **Discovery** | ESP/Tasmota **mDNS** (commissionable `_matterc._udp` + operational `_matter._tcp`) | Reuse existing stack |
| **Persistence** | Tasmota **UFS** — compact binary fabric/session store | Survives reboot, multi-fabric |
| **Build gate** | `USE_MATTER_C`, **off by default**, own Xdrv slot (TBD) | Isolation; opt-in |
| **Targets** | ESP32 / S3 / C6 (WiFi commissioning; **Thread deferred**) | Matches available hardware |

---

## 2. Architecture — structural skeleton (mirrored in concept, re-derived in code)

```
mtrc_module        ─ entry point, xdrv hooks, lifecycle
  mtrc_device      ─ root node, endpoint registry, attribute store
  ── transport ──
  mtrc_tlv         ─ TLV encode/decode (the codec everything rides on)
  mtrc_frame       ─ message header parse/build
  mtrc_mrp         ─ Message Reliability Protocol (ack/retransmit/counters)
  mtrc_udp         ─ UDP operational
  mtrc_btp         ─ BLE Transport Protocol (commissioning)
  ── security ──
  mtrc_crypto      ─ thin BearSSL wrappers (AES-CCM, HKDF, P-256, raw ECDSA)
  mtrc_spake2p     ─ PASE handshake (spec assembly on BearSSL muladd)
  mtrc_case        ─ CASE handshake (operational session)
  mtrc_session     ─ session table + key schedule
  mtrc_fabric      ─ fabric table, NOC/ICAC/RCAC, ACL
  mtrc_store       ─ UFS persistence
  ── interaction model ──
  mtrc_im          ─ read / write / invoke / subscribe dispatch
  mtrc_path        ─ endpoint/cluster/attribute path resolution
  ── device plugins ──
  mtrc_ep_*        ─ one file per device type (onoff, light, sensor_*, …)
  ── onboarding ──
  mtrc_qr          ─ Base38 + Verhoeff + QR / manual pairing code
```

---

## 3. Feature scope — three parity tiers

**Tier A — MVP vertical slice (the proof):**
Root node + **OnOff** (relay/plug). One device, commissioned and toggled from
`chip-tool`. Exercises the entire hard path — TLV, frame, MRP, SPAKE2+,
BTP/BLE, CASE, fabric store, IM-invoke, one cluster. If this works, the
project is de-risked.

**Tier B — Berry-parity core (the goal):**
- Lights: OnOff, Dimmable, ColorTemp, Color (HS + XY)
- Sensors: Temperature, Humidity, Pressure, Illuminance, Occupancy, Contact, Flow
- GenericSwitch (button), Fan, Window covering (Shutter)

**Tier C — stretch:** Aggregator/Bridge (expose multiple Tasmota relays/sensors
as one bridge node), Thermostat, Air Quality.

**Explicitly skipped:** HTTP_remote (Tasmota-specific), Zigbee bridge,
Profiler, Thread radio.

---

## 4. Phased roadmap (risk-first)

| Phase | Deliverable | Exit criterion |
|---|---|---|
| **0. Spike ✅ DONE** | `mtrc_crypto` (BearSSL SHA256/HMAC/HKDF + P-256 mul/mulgen/muladd) + `mtrc_spake2p` | ✅ **PASS — GO.** All 11 checks match the RFC 9383 P256 vector (X, Y, Z, V both sides, K_main, K_confirmP/V, cA, cB) on `br_ec_p256_m15`. Host self-test: `test/build_host.sh`. Firmware co-build verified on tinyc32c3-matter (83.9% flash). |
| **1. TLV ✅ DONE** | `mtrc_tlv` encode/decode (streaming, zero-copy reader) | ✅ PASS. 14 canonical spec encode vectors match; full nested round-trip decode + byte-identical re-encode; fully-qualified tags. Host test `test/build_tlv.sh`; firmware co-build on tinyc32c3-matter. |
| **2. Frame+MRP ✅ DONE** | `mtrc_frame` (message+protocol header codec) + `mtrc_mrp` (reliability) | ✅ PASS. Canonical frame bytes + full-header round-trip; MRP reliable-send→ack clears pending, dropped-ack→4 backoff retransmits→timeout, sliding-window dedup. Host test `test/build_msg.sh`. UDP socket glue deferred to firmware port (Phase 6). |
| **3. Commissioning slice** | PASE, BTP/BLE, CASE, fabric store, **OnOff** | ⭐ **chip-tool pairs over BLE→WiFi and toggles the relay.** The milestone. |
| **4. IM breadth** | read/write/subscribe (not just invoke) | chip-tool reads attrs + receives subscription reports |
| **5. Cluster breadth** | Tier B device types | Each commissions + functions in chip-tool |
| **6. Tasmota integration** | Relay/light/sensor bridge, web UI, settings, QR on display | Real Tasmota relay & sensor exposed; QR pairs from Apple Home |
| **7. Hardening** | Multi-fabric/admin, robustness, (optional) cert path | Pairs simultaneously to Apple + Google; survives soak |

---

## 5. Testing & interop strategy

- **Unit:** TLV + SPAKE2+ + key-schedule against **spec test vectors** (the
  spec ships known vectors — invaluable, and keeps provenance clean).
- **Integration:** `chip-tool` (CSA reference controller) on Mac/Linux — daily driver.
- **Ecosystems:** Apple Home, Google Home, Alexa, Home Assistant (matter-server).
- **VID/PID:** Matter **test VID** (0xFFF1–0xFFF4) + dev PID for non-cert interop.
- **Hardware:** `.39` (S3) primary DUT. Session-crypto bugs will be
  StoreProhibited/heap crashes → use the JTAG `-dbg` env + **hardware
  watchpoints** (see memory `jtag_watchpoint_debugging.md`).

---

## 6. Flash & RAM budget — the whole point, with checkpoints

BearSSL is already linked in TLS builds, so crypto core = **~0 incremental
flash** and **low, malloc-free RAM**. We charge only our wrappers + protocol.

| Checkpoint | Flash target | Action if exceeded |
|---|---|---|
| End Phase 3 (core + OnOff) | **< 60 kB** | architecture too heavy — review before adding clusters |
| End Tier B (Berry-parity) | **< 100 kB** | should clearly beat HomeKit's 156 kB |
| Stretch w/ Bridge | < 130 kB | still under HomeKit |

**RAM:** BearSSL i15 keeps per-handshake bignum scratch on the stack
(hundreds of bytes), not heap — the decisive advantage over mbedTLS under
WiFi+web+VM heap pressure. Track free-heap during a commissioning soak.

Measure flash with `obj-dump.py` (already in the build) after each phase.

---

## 7. Top risks & mitigations

1. **SPAKE2+ correctness** (subtle assembly) → spec test vectors in Phase 0,
   before any Matter protocol code.
2. **BearSSL `br_ec` surface** → confirm `muladd` gives `x·P + w0·M` on P-256
   cleanly (it should — `muladd` = `A·x + B·y`). Verify M/N point import via
   `br_ec`'s uncompressed-point format in the Phase-0 spike.
3. **BTP windowing** (Matter's BLE fragmentation is fiddly) → port from the
   spec section, debug with chip-tool BLE verbose logs.
4. **lwIP IPv6** must be enabled in the build → confirm in Phase 2.
5. **Schedule risk concentrated in Phase 3.** If PASE+CASE work, the rest is
   mechanical breadth.

**Honest effort estimate:** ~4–5 months part-time to Tier B interop. Phase 3
is ~40% of total risk and effort.

---

## 8. Concrete steps

1. ✅ Scaffold `lib/libesp32_div/matter_c/` + `USE_MATTER_C` gate + dev envs.
2. ✅ **Phase-0 crypto spike** — `mtrc_crypto` over BearSSL + `mtrc_spake2p`,
   verified 11/11 against the RFC 9383 P256 vector (host + firmware co-build).
   Viability confirmed: the hard crypto works on the resident library.
3. ✅ **Phase 1 `mtrc_tlv`** — Matter TLV codec, 14 spec vectors + round-trip.
4. ✅ **Phase 2 `mtrc_frame` + `mtrc_mrp`** — message/protocol header codec
   + reliability (ack, backoff retransmit, dedup). 18 checks PASS.
5. ⏳ **Phase 3 (the wall) — IN PROGRESS**: commissioning vertical slice.
   - ✅ **3a PASE + key schedule** (`mtrc_pase`): PBKDF2 w0/w1, the Matter
     SPAKE2+ key schedule (context = SHA256("CHIP PAKE V1 Commissioning"
     ‖req‖resp), Ka/Ke split, ConfirmationKeys, SessionKeys), and the
     PBKDFParamReq/Resp + Pake1/2/3 TLV codecs. 24 checks PASS, incl. a
     full prover↔verifier handshake deriving identical I2R/R2I/Att keys.
     Matter constants taken verbatim from connectedhomeip.
   - ✅ **3b secured frame** (`mtrc_sec` + `mtrc_crypto` AES-CCM): AES-128-CCM
     encrypt/decrypt with the message header as AAD, 13-byte Matter nonce
     (secflags‖ctr‖srcnode), 16-byte MIC, keyed by the PASE session keys.
     16 checks PASS — CCM matches a Python AESCCM reference byte-exact;
     secured round-trip recovers header/proto/payload; tampered ciphertext,
     tampered header(AAD), and wrong key all fail the MIC. `mtrc_frame`
     refactored into header/proto split codecs. Privacy(§4.8) deferred
     (optional). Host test `test/build_sec.sh`.
   - ⏭ **3c BTP/BLE** transport · **3d CASE** · **3e fabric store** ·
     **3f OnOff endpoint**. Exit: chip-tool pairs over BLE→WiFi, toggles relay.

### How to re-run the host self-tests
```
bash lib/libesp32_div/matter_c/test/build_host.sh   # SPAKE2+   -> "PASS — GO"
bash lib/libesp32_div/matter_c/test/build_tlv.sh    # TLV codec -> "PASS"
bash lib/libesp32_div/matter_c/test/build_msg.sh    # frame+MRP -> "PASS"
bash lib/libesp32_div/matter_c/test/build_pase.sh   # PASE      -> "PASS"
bash lib/libesp32_div/matter_c/test/build_sec.sh    # AES-CCM   -> "PASS"
```

---

## 9. Open decisions

- [ ] Which Xdrv slot is free in the fork for `USE_MATTER_C`?
- [ ] Dev VID/PID for interop testing (test VID 0xFFF1–0xFFF4 + chosen PID).
- [ ] Confirm lwIP IPv6 is (or can be) enabled in the target build envs.
