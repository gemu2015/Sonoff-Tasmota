#!/usr/bin/env python3
"""slcan_loopback_test.py — verify the SLCAN bridge end-to-end without
a CAN transceiver.

Setup:
  1. Flash a tinyc32c3 build to your ESP32-C3.
  2. Edit `tasmota/tinyc/examples/slcan_bridge_tcp.tc` (or
     `slcan_bridge.tc` for the serial variant):
       int  twai_mode  = 1;          // ← change 0 to 1 (NO_ACK)
     Recompile + upload via tc_deploy.mjs.
  3. NO jumper wire needed. ESP32 TWAI's NO_ACK mode (Self Test Mode)
     receives its own transmissions internally via the controller's
     loopback path — no transceiver, no second node, no wire.
  4. Connect to the bridge:
       — TCP variant:   target = '<bridge_ip>:8888'
       — serial variant: target = '/dev/cu.usbserial-*' (or whatever
         port your USB-UART adapter shows up as)
  5. Run this script.

What it tests (✓) and does NOT test (✗):
  ✓ Transport: TCP socket OR USB-serial RX+TX, Mac ↔ bridge
  ✓ SLCAN ASCII command parser (S/O/C/t/T/V/F)
  ✓ TinyC twaiBegin / twaiSend / twaiRecv plumbing
  ✓ TWAI driver in NO_ACK / Self Test mode (internal loopback)
  ✓ Frame format round-trip:
      11-bit IDs, 29-bit IDs, DLCs 0..8, payload byte fidelity
  ✗ CAN transceiver electricals — no transceiver in the loop
  ✗ Differential bus signalling — TWAI is in self-test mode
  ✗ Bus arbitration with peer nodes
  ✗ Bus-off / error-recovery behaviour

The remaining ✗ items get exercised once the level converters arrive
and a real DUT is on the bus.

Usage:
  python slcan_loopback_test.py 192.168.188.143:8888 [--bitrate 250]
  python slcan_loopback_test.py /dev/cu.usbserial-* [--bitrate 250]
"""

from __future__ import annotations
import argparse
import random
import sys
import time
from typing import List

from slcan_client import SlcanClient, CanFrame, SLCAN_BITRATES


def _build_test_frames(n: int) -> List[CanFrame]:
    """Generate `n` deterministic-ish test frames covering the
    interesting edges of the protocol."""
    rng = random.Random(0xCAFE)
    frames: List[CanFrame] = []

    # Anchor cases — fixed so any regression has the same fingerprint.
    frames.append(CanFrame(0x000, False, b''))                   # min std, dlc 0
    frames.append(CanFrame(0x7FF, False, b'\xFF' * 8))           # max std, full payload
    frames.append(CanFrame(0x000_0000, True,  b''))              # min ext, dlc 0
    frames.append(CanFrame(0x1FFFFFFF, True, b'\xAA\x55\xAA\x55\xAA\x55\xAA\x55'))
    frames.append(CanFrame(0x123, False, bytes(range(8))))       # std, ascending payload
    frames.append(CanFrame(0x18FF50E5, True, bytes.fromhex('0102030405')))  # 5-byte ext (Sorel-ish)
    frames.append(CanFrame(0x1081407F, True, bytes.fromhex('08017000005AA55A')))  # 8-byte ext (Huawei R4850-style)

    # Random fill up to `n` for breadth.
    while len(frames) < n:
        ext = rng.choice([False, True])
        max_id = 0x1FFFFFFF if ext else 0x7FF
        cid = rng.randint(0, max_id)
        dlc = rng.randint(0, 8)
        data = bytes(rng.randint(0, 255) for _ in range(dlc))
        frames.append(CanFrame(cid, ext, data))

    return frames[:n]


def _frame_eq(a: CanFrame, b: CanFrame) -> bool:
    return a.id == b.id and a.ext == b.ext and a.data == b.data


def _frame_str(f: CanFrame) -> str:
    tag = 'EXT' if f.ext else 'STD'
    width = 8 if f.ext else 3
    return f"{tag} 0x{f.id:0{width}x} dlc={len(f.data)} {f.data.hex(' ')}"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('target', nargs='?', default='/dev/cu.usbmodem*',
                    help="serial port glob (default '/dev/cu.usbmodem*') "
                         "or TCP 'host:port' (e.g. 192.168.188.143:8888)")
    ap.add_argument('--bitrate', type=int, default=250,
                    choices=SLCAN_BITRATES, help='kbit/s (must match bridge)')
    ap.add_argument('--frames', type=int, default=50,
                    help='how many frames to round-trip (default 50)')
    ap.add_argument('--per-frame-timeout', type=float, default=0.5,
                    help='seconds to wait for each loopback (default 0.5)')
    ap.add_argument('--quiet', action='store_true',
                    help='only print summary')
    args = ap.parse_args()

    print(f"slcan_loopback_test: opening {args.target} @ {args.bitrate} kbit/s")
    print(f"  Make sure twai_mode=1 in slcan_bridge[_tcp].tc on the "
          f"bridge ESP32 — TWAI Self Test mode handles loopback "
          f"internally, no jumper wire needed.\n")

    test_frames = _build_test_frames(args.frames)
    passed = 0
    failed = 0
    drops  = 0

    try:
        with SlcanClient(args.target, bitrate=args.bitrate) as c:
            for i, sent in enumerate(test_frames, 1):
                ack = c.send(sent.id, sent.ext, sent.data, timeout=0.3)
                if not ack:
                    if not args.quiet:
                        print(f"  [{i:3}/{len(test_frames)}] TX NACK   {_frame_str(sent)}")
                    failed += 1
                    continue

                # Wait for the loopback echo. NO_ACK mode + jumper means
                # we should see exactly one frame come back per send,
                # with the same payload.
                got = c.recv(timeout=args.per_frame_timeout)
                if got is None:
                    if not args.quiet:
                        print(f"  [{i:3}/{len(test_frames)}] DROP (no rx within {args.per_frame_timeout}s)  {_frame_str(sent)}")
                    drops += 1
                    continue

                if _frame_eq(sent, got):
                    if not args.quiet:
                        print(f"  [{i:3}/{len(test_frames)}] OK  {_frame_str(sent)}")
                    passed += 1
                else:
                    print(f"  [{i:3}/{len(test_frames)}] MISMATCH")
                    print(f"      sent: {_frame_str(sent)}")
                    print(f"      got:  {_frame_str(got)}")
                    failed += 1

    except (FileNotFoundError, OSError, IOError) as e:
        print(f"\nslcan_loopback_test: FATAL — {e}")
        print("  Verify (1) the bridge is flashed + slcan_bridge.tc is running,")
        print("  (2) USB cable connected, (3) port glob matches the right device.")
        return 2

    total = len(test_frames)
    print()
    print(f"=== Loopback summary ===")
    print(f"  Total:    {total}")
    print(f"  Passed:   {passed}")
    print(f"  NACKed:   {failed}")
    print(f"  Dropped:  {drops}")
    if passed == total:
        print("  Result:   ✅ ALL PASS — bridge protocol round-trips cleanly.")
        return 0
    elif passed >= total * 0.95:
        print("  Result:   🟡 mostly passing — investigate occasional drops.")
        return 1
    else:
        print("  Result:   ❌ FAIL — bridge / wiring / mode is wrong.")
        return 1


if __name__ == '__main__':
    sys.exit(main())
