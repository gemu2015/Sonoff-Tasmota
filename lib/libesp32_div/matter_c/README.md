# matter_c — a pure-C Matter 1.4 device for Tasmota + TinyC

A from-scratch, pure-C Matter 1.4 device stack for the gemu2015/Sonoff-Tasmota
fork. The C core is a **generic Matter engine**; a **TinyC script defines the
device** — it declares endpoints/clusters, publishes attribute values and
handles commands, with no firmware rebuild. Crypto rides the already-resident
**BearSSL** (no mbedTLS), so the incremental flash is small and per-handshake
bignum scratch stays on the stack rather than the heap.

> **Status:** working end-to-end. Commissions and operates with the CSA
> reference controller (**chip-tool**, full PASE → attestation → CSR → AddNOC →
> CASE over IPv6) **and Apple Home** — an On/Off plug + an Extended Color Light
> commission, appear as separate accessories, and control (on/off, brightness,
> colour) live. Fabrics persist on UFS across reboots; multiple fabrics and
> concurrent controller (iPhone + HomePods/Apple TV) sessions are supported.

**Provenance:** *inspired by* (not converted from) the device-subset
architecture of Tasmota Berry Matter (S. Hadinger). Wire format / protocol is
implemented from the CSA Matter 1.4.1 spec; pure algorithms (SPAKE2+, CASE,
Base38, Verhoeff, QR) are re-derived from the spec; crypto calls BearSSL
(T. Pornin, BSD) directly. Namespace `mtrc_`, C-native idioms. **GPLv3.**

---

## Architecture

```
matter_c.c        engine: lifecycle, RX ring, sessions, IM dispatch, scripting API
mtrc_tlv          Matter TLV encode/decode
mtrc_frame        message + protocol header codec
mtrc_mrp          Message Reliability Protocol (ack / backoff retransmit / dedup)
mtrc_crypto       thin BearSSL wrappers (AES-CCM, HKDF/HMAC/SHA-256, P-256, raw ECDSA)
mtrc_spake2p      PASE handshake (SPAKE2+ assembly on BearSSL muladd)
mtrc_case         CASE handshake + operational key schedule
mtrc_case_msg     Sigma1/2/3 + TBE/TBS message codecs
mtrc_sec          secured (AES-CCM) frame encode/decode
mtrc_cert         compact-TLV operational cert (RCAC/ICAC/NOC) parse
mtrc_csr          PKCS#10 CSR builder
mtrc_store        fabric table + UFS serialization (persists across reboot)
mtrc_dm           data-model registry (endpoints / clusters / attributes)
mtrc_im           Interaction Model (Read / Write / Subscribe / Invoke / Events)
qrcodegen         on-device SVG pairing QR
```

The host integration (mDNS, UDP:5540, UFS kv store, relay/LED bridge) is the
`matter_port_t` struct in `include/matter_c.h`, wired in
`tasmota/tasmota_xdrv_driver/xdrv_124_tinyc.ino`. Copy the folder + implement
the port to reuse on another firmware.

---

## TinyC scripting API

| Builtin | Purpose |
|---|---|
| `matterReset()` | clear the data model (root node only) |
| `matterAdd(deviceType)` → ep | add an endpoint (auto-adds Descriptor + Identify + the type's mandatory clusters) |
| `matterCluster(ep, cluster)` | add a cluster to an endpoint |
| `matterAttr(ep, cl, attr, type)` | declare an attribute (`MTR_BOOL/U8/U16/U32/S16/S32/S64/FLOAT/…`) |
| `matterSet(ep, cl, attr, value)` | set an integer/bool attribute |
| `matterSetFloat(ep, cl, attr, value, scale)` | set a value as `round(value*scale)` (S64) or float bits (FLOAT) |
| `matterGet(ep, cl, attr)` | read an attribute back |
| `matterEvent(ep, cl, eventId, a, b)` | emit a Matter event (e.g. Generic Switch press) |
| `matterStart()` | go operational (publish operational mDNS for stored fabrics) |
| `MatterInvoke(ep, cl, cmd)` | callback: a controller invoked a command (OnOff/Level/Color/…) |
| `EverySecond()` | callback: push live sensor/meter values |

Pairing is opened from the **`/mt`** web page (Bind → 10-min window + on-device
QR); `/mt?unbind=1` factory-resets the fabrics. The device is not openly
pairable until Bind.

---

## Examples (`tasmota/tinyc/examples/`)

| Example | Device type(s) |
|---|---|
| `matter_plug.tc` | On/Off Plug-in Unit (relay) + Electrical Power Measurement |
| `matter_rgb.tc` | On/Off plug + Extended Color Light (WS2812) |
| `matter_sensors.tc` | Temperature + Humidity + Pressure (float) |
| `matter_powermeter.tc` | Electrical Sensor — power/voltage/current + cumulative energy (SML) |
| `matter_fan.tc` | Fan Control |
| `matter_shutter.tc` | Window Covering |
| `matter_leak.tc` | Water-Leak + Rain (Boolean State) |
| `matter_airquality.tc` | Air Quality — CO2 / PM2.5 / TVOC (float) |
| `matter_button.tc` | Generic Switch (events: single / double / long press) |

Deploy with `node tasmota/tinyc/tc_deploy.mjs examples/<name>.tc <device-ip>`.

---

## Build & flash

Gate: `USE_MATTER_C` (mutually exclusive with `USE_HOMEKIT`). The local
`platformio_override.ini` envs `tinyc32c6-matter` / `tinyc32s3-matter` /
`tinyc32c3-matter` add `-DTINYC_MATTER` and unflag `-DTINYC_HOMEKIT`.

```
pio run -e tinyc32c6-matter                              # build
bash lib/libesp32_div/matter_c/test/flash_c6.sh <ip>    # safeboot-aware OTA
```

Primary DUT: ESP32-C6. Targets ESP32 / S3 / C6 over Wi-Fi/IP (BLE and Thread
are intentionally not used — Tasmota is already on the network).

---

## Debugging

Build with `-DMTRC_DIAG` to enable verbose debug logging (data-model endpoint
dump at `matterStart`, raw IM StatusResponse decode). Off in normal builds.

```
PLATFORMIO_BUILD_FLAGS="-DMTRC_DIAG" pio run -e tinyc32c6-matter
```

## Host self-tests

Pure-algorithm units run on the host (no device needed):

```
bash test/build_host.sh      # SPAKE2+      bash test/build_case.sh      # CASE keys
bash test/build_tlv.sh       # TLV codec    bash test/build_case_msg.sh  # CASE msgs
bash test/build_msg.sh       # frame + MRP  bash test/build_cert.sh      # cert parse
bash test/build_pase.sh      # PASE         bash test/build_csr.sh       # CSR builder
bash test/build_sec.sh       # AES-CCM      bash test/build_store.sh     # fabric store
bash test/build_ec.sh        # ECDH/ECDSA   bash test/build_dm.sh        # data model
```
