#!/usr/bin/env python3
"""pase_probe.py — run a Matter PASE handshake against a device, no controller needed.

    /usr/bin/python3 tasmota/Plugins/matter/tools/pase_probe.py <ip> [passcode] [--read ep:cluster:attr ...]

Plays the commissioner side of PASE (Core Spec §4.14.1) over UDP 5540:
PBKDFParamRequest -> PBKDFParamResponse, Pake1 -> Pake2, Pake3 -> StatusReport.
It checks the device's confirmation cB against its own computation and ends
with the device's StatusReport, so a pass means the device's whole SPAKE2+ path
is right: PBKDF2, P-256 point math (incl. the SPAKE M/N points and the group
order), the transcript, HKDF and HMAC. That is exactly what the MATTERF plugin
moved around (tables, texts, heap block), so run it once against the built-in
Matter and once against the plugin.

After the handshake it opens the PASE secure session (AES-CCM, pure Python)
and exercises the commissioning commands a controller sends next: it reads
BasicInformation, fetches the DAC and PAI (CertificateChainRequest, compared
with lib/.../mtrc_attest_creds.h) and checks the AttestationResponse signature
against the DAC public key.

The commissioning window must be open (web /mt: Bind). Pure Python — the stock
/usr/bin/python3 is the one that may talk to the LAN on this Mac, and it has no
`cryptography` package; P-256 is done by hand (slow, but a handful of scalar
multiplications). Talks IPv4; the device then answers the last IPv4 peer.
"""
import hashlib
import hmac
import os
import re
import socket
import struct
import sys
import time

# ---- P-256 ------------------------------------------------------------------
P = 0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff
N = 0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551
A = P - 3
G = (0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296,
     0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5)


def ec_add(p1, p2):
    if p1 is None: return p2
    if p2 is None: return p1
    (x1, y1), (x2, y2) = p1, p2
    if x1 == x2 and (y1 + y2) % P == 0:
        return None
    if p1 == p2:
        l = (3 * x1 * x1 + A) * pow(2 * y1, P - 2, P) % P
    else:
        l = (y2 - y1) * pow(x2 - x1, P - 2, P) % P
    x3 = (l * l - x1 - x2) % P
    return (x3, (l * (x1 - x3) - y1) % P)


def ec_mul(k, pt):
    r = None
    for bit in bin(k % N)[2:]:
        r = ec_add(r, r)
        if bit == "1":
            r = ec_add(r, pt)
    return r


def ec_neg(pt):
    return (pt[0], (-pt[1]) % P)


def enc(pt):
    return b"\x04" + pt[0].to_bytes(32, "big") + pt[1].to_bytes(32, "big")


def dec(b):
    assert len(b) == 65 and b[0] == 4
    return (int.from_bytes(b[1:33], "big"), int.from_bytes(b[33:], "big"))


def spake_points():
    """SPAKE2+ M and N from the matter sources (same bytes the device uses)"""
    src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src",
                            "mtrc_spake2p_c.h"), encoding="utf-8").read()
    out = {}
    for name in ("SPAKE_M", "SPAKE_N"):
        body = re.search(name + r"\s*,\s*\[65\]\)\s*=\s*\{(.*?)\};", src, re.S) or \
               re.search(name + r"\[65\]\s*=\s*\{(.*?)\};", src, re.S)
        out[name] = dec(bytes(int(h, 16) for h in re.findall(r"0x([0-9a-fA-F]{2})", body.group(1))))
    return out["SPAKE_M"], out["SPAKE_N"]


# ---- HKDF / TLV ---------------------------------------------------------------
def hkdf(ikm, info, n, salt=b""):
    prk = hmac.new(salt or b"\0" * 32, ikm, hashlib.sha256).digest()
    okm, t, i = b"", b"", 1
    while len(okm) < n:
        t = hmac.new(prk, t + info + bytes([i]), hashlib.sha256).digest()
        okm += t; i += 1
    return okm[:n]


def tlv_bytes(tag, b):
    return bytes([0x30, tag, len(b)]) + b if len(b) < 256 else bytes([0x31, tag]) + struct.pack("<H", len(b)) + b


def tlv_parse(b):
    """flat {tag: value} of the outer anonymous struct; nested structs -> dict"""
    def parse(i):
        out = {}
        while i < len(b):
            c = b[i]; i += 1
            if c == 0x18:
                return out, i
            tagc, typ = c >> 5, c & 0x1F
            tag = None
            if tagc == 1:
                tag = b[i]; i += 1
            if typ in (0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03):
                n = 1 << (typ & 3)
                v = int.from_bytes(b[i:i + n], "little"); i += n
            elif typ in (0x08, 0x09):
                v = typ == 0x09
            elif typ in (0x10, 0x11, 0x0C, 0x0D):
                ln = 1 << (typ & 3)
                n = int.from_bytes(b[i:i + ln], "little"); i += ln
                v = b[i:i + n]; i += n
            elif typ in (0x15, 0x16, 0x17):
                v, i = parse(i)
            else:
                raise ValueError(f"TLV type 0x{typ:02x}")
            out[tag] = v
        return out, i
    assert b[0] == 0x15
    return parse(1)[0]



# ---- AES-128 / CCM (encrypt direction only is needed) ------------------------
SBOX = [0] * 256
def _init_sbox():
    p = q = 1
    while True:
        p = p ^ ((p << 1) & 0xFF) ^ (0x1B if p & 0x80 else 0)
        q ^= q << 1; q ^= q << 2; q ^= q << 4; q &= 0xFF
        if q & 0x80: q ^= 0x09
        x = q ^ ((q << 1) | (q >> 7)) ^ ((q << 2) | (q >> 6)) ^ ((q << 3) | (q >> 5)) ^ ((q << 4) | (q >> 4))
        SBOX[p] = (x ^ 0x63) & 0xFF
        if p == 1: break
    SBOX[0] = 0x63
_init_sbox()


def _xt(a):
    return ((a << 1) ^ 0x1B) & 0xFF if a & 0x80 else a << 1


def aes_encrypt_block(key, blk):
    rk = list(key); rcon = 1
    for i in range(16, 176, 4):
        t = rk[i - 4:i]
        if i % 16 == 0:
            t = [SBOX[t[1]] ^ rcon, SBOX[t[2]], SBOX[t[3]], SBOX[t[0]]]
            rcon = _xt(rcon)
        rk += [rk[i - 16 + j] ^ t[j] for j in range(4)]
    s = [blk[i] ^ rk[i] for i in range(16)]
    for rnd in range(1, 11):
        s = [SBOX[b] for b in s]
        s = [s[(i + 4 * (i % 4)) % 16] for i in range(16)]           # ShiftRows
        if rnd != 10:
            n = []
            for c in range(4):
                a = s[4 * c:4 * c + 4]
                t = a[0] ^ a[1] ^ a[2] ^ a[3]
                n += [a[j] ^ t ^ _xt(a[j] ^ a[(j + 1) % 4]) for j in range(4)]
            s = n
        s = [s[i] ^ rk[16 * rnd + i] for i in range(16)]
    return bytes(s)


def ccm(key, nonce, data, aad, decrypt=False, tag=None):
    """AES-CCM, 13-byte nonce (L = 2), 16-byte MIC"""
    def ctr(i):
        return aes_encrypt_block(key, bytes([1]) + nonce + i.to_bytes(2, "big"))
    stream = b"".join(ctr(i + 1) for i in range((len(data) + 15) // 16))
    pt = bytes(a ^ b for a, b in zip(data, stream)) if decrypt else data
    b0 = bytes([(0x40 if aad else 0) | (((16 - 2) // 2) << 3) | 1]) + nonce + len(pt).to_bytes(2, "big")
    blocks = b0
    if aad:
        a = len(aad).to_bytes(2, "big") + aad
        blocks += a + b"\0" * (-len(a) % 16)
    blocks += pt + b"\0" * (-len(pt) % 16)
    x = bytes(16)
    for i in range(0, len(blocks), 16):
        x = aes_encrypt_block(key, bytes(a ^ b for a, b in zip(x, blocks[i:i + 16])))
    mic = bytes(a ^ b for a, b in zip(x, ctr(0)))
    if decrypt:
        if mic != tag:
            raise ValueError("MIC mismatch")
        return pt
    return bytes(a ^ b for a, b in zip(data, stream)) + mic


# ---- generic TLV (for the Interaction Model) ------------------------------------
def t_uint(tag, v):
    for n, t in ((1, 4), (2, 5), (4, 6), (8, 7)):
        if v < (1 << (8 * n)):
            return (bytes([0x20 | t, tag]) if tag is not None else bytes([t])) + v.to_bytes(n, "little")


def t_bool(tag, v):
    return bytes([0x20 | (9 if v else 8), tag])


def t_cont(kind, tag, *items):          # kind: 0x15 struct, 0x16 array, 0x17 list
    return (bytes([0x20 | kind, tag]) if tag is not None else bytes([kind])) + b"".join(items) + b"\x18"


def tlv_tree(b, i=0):
    """-> (value, next); containers become [(tag, value), ...]"""
    c = b[i]; i += 1
    tagc, typ = c >> 5, c & 0x1F
    tag = None
    if tagc == 1: tag = b[i]; i += 1
    elif tagc in (2, 3): tag = int.from_bytes(b[i:i + 2], "little"); i += 2
    elif tagc in (6, 7): tag = int.from_bytes(b[i:i + 4], "little"); i += 4
    if typ <= 0x07:
        n = 1 << (typ & 3); v = int.from_bytes(b[i:i + n], "little", signed=typ < 4); i += n
    elif typ in (0x08, 0x09): v = typ == 0x09
    elif typ in (0x0A, 0x0B):
        n = 4 if typ == 0x0A else 8; v = struct.unpack("<f" if n == 4 else "<d", b[i:i + n])[0]; i += n
    elif 0x0C <= typ <= 0x13:
        ln = 1 << (typ & 3); n = int.from_bytes(b[i:i + ln], "little"); i += ln
        v = b[i:i + n]; i += n
        if typ < 0x10: v = v.decode("utf-8", "replace")
    elif typ == 0x14: v = None
    elif typ in (0x15, 0x16, 0x17):
        v = []
        while b[i] != 0x18:
            (t2, v2), i = tlv_tree(b, i)
            v.append((t2, v2))
        i += 1
    else:
        raise ValueError(f"TLV type 0x{typ:02x}")
    return (tag, v), i


def tget(cont, *path):
    for t in path:
        cont = next(v for tg, v in cont if tg == t)
    return cont

# ---- message framing ------------------------------------------------------------
class Link:
    def __init__(self, ip):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.settimeout(15)
        self.dst = (ip, 5540)
        self.ctr = int.from_bytes(os.urandom(4), "little") & 0x0FFFFFFF
        self.node = os.urandom(8)
        self.exch = int.from_bytes(os.urandom(2), "little")
        self.ack = None

    def send(self, opcode, payload, reliable=True):
        self.ctr += 1
        hdr = bytes([0x04]) + struct.pack("<HBI", 0, 0, self.ctr) + self.node
        xf = 0x01 | (0x04 if reliable else 0) | (0x02 if self.ack is not None else 0)
        ph = bytes([xf, opcode]) + struct.pack("<HH", self.exch, 0)
        if self.ack is not None:
            ph += struct.pack("<I", self.ack)
        self.s.sendto(hdr + ph + payload, self.dst)

    def recv(self, want):
        end = time.time() + 15
        while time.time() < end:
            d, _ = self.s.recvfrom(1500)
            mf = d[0]; i = 1
            sid, sf, ctr = struct.unpack_from("<HBI", d, i); i += 7
            if mf & 0x04: i += 8
            dsiz = mf & 3
            i += 8 if dsiz == 1 else 2 if dsiz == 2 else 0
            xf, op = d[i], d[i + 1]; i += 6
            if xf & 0x10: i += 2
            if xf & 0x02: i += 4
            if xf & 0x04:
                self.ack = ctr
            if op == 0x10:            # standalone ack
                continue
            if op != want:
                raise RuntimeError(f"expected opcode 0x{want:02x}, got 0x{op:02x}: {d[i:].hex()}")
            return d[i:]
        raise TimeoutError(f"no opcode 0x{want:02x}")



class Secure:
    """PASE secure session: I2R encrypts our messages, R2I the device's"""
    def __init__(self, lk, dev_sid, i2r, r2i):
        self.lk, self.sid, self.i2r, self.r2i = lk, dev_sid, i2r, r2i
        self.ctr = int.from_bytes(os.urandom(4), "little") & 0x0FFFFFFF
        self.exch = lk.exch
        self.ack = None

    def send(self, proto, opcode, payload, new_exchange=False, reliable=True):
        if new_exchange:
            self.exch = (self.exch + 1) & 0xFFFF
        self.ctr += 1
        hdr = bytes([0x00]) + struct.pack("<HBI", self.sid, 0, self.ctr)
        xf = 0x01 | (0x04 if reliable else 0) | (0x02 if self.ack is not None else 0)
        ph = bytes([xf, opcode]) + struct.pack("<HH", self.exch, proto)
        if self.ack is not None:
            ph += struct.pack("<I", self.ack); self.ack = None
        nonce = bytes([0]) + struct.pack("<I", self.ctr) + bytes(8)
        self.lk.s.sendto(hdr + ccm(self.i2r, nonce, ph + payload, hdr), self.lk.dst)

    def recv(self, want_proto, want_op):
        end = time.time() + 20
        while time.time() < end:
            d, _ = self.lk.s.recvfrom(1500)
            mf = d[0]; i = 1
            sid, sf, ctr = struct.unpack_from("<HBI", d, i); i += 7
            if mf & 0x04: i += 8
            dsiz = mf & 3
            i += 8 if dsiz == 1 else 2 if dsiz == 2 else 0
            if sid == 0:
                continue                                   # unsecured leftovers
            nonce = bytes([sf]) + struct.pack("<I", ctr) + bytes(8)
            pt = ccm(self.r2i, nonce, d[i:-16], d[:i], decrypt=True, tag=d[-16:])
            xf, op, ex, proto = pt[0], pt[1], *struct.unpack_from("<HH", pt, 2)
            j = 6 + (2 if xf & 0x10 else 0) + (4 if xf & 0x02 else 0)
            if xf & 0x04:
                self.ack = ctr
            if op == 0x10 and proto == 0:
                continue
            if (proto, op) != (want_proto, want_op):
                raise RuntimeError(f"expected {want_proto:#x}/{want_op:#x}, got {proto:#x}/{op:#x}")
            return pt[j:]
        raise TimeoutError(f"no {want_proto:#x}/{want_op:#x}")

    def invoke(self, ep, cluster, cmd, fields):
        req = t_cont(0x15, None, t_bool(0, False), t_bool(1, False),
                     t_cont(0x16, 2, t_cont(0x15, None,
                         t_cont(0x17, 0, t_uint(0, ep), t_uint(1, cluster), t_uint(2, cmd)),
                         t_cont(0x15, 1, *fields))),
                     t_uint(0xFF, 12))
        self.send(1, 0x08, req, new_exchange=True)
        (_, resp), _ = tlv_tree(self.recv(1, 0x09))
        self.send(0, 0x10, b"", reliable=False)             # ack
        return tget(tget(resp, 1)[0][1], 0)                 # first InvokeResponseIB -> CommandDataIB

    def read(self, ep, cluster, attrs):
        paths = [t_cont(0x17, None, t_uint(2, ep), t_uint(3, cluster), t_uint(4, a)) for a in attrs]
        req = t_cont(0x15, None, t_cont(0x16, 0, *paths), t_bool(3, True), t_uint(0xFF, 12))
        self.send(1, 0x02, req, new_exchange=True)
        (_, rep), _ = tlv_tree(self.recv(1, 0x05))
        self.send(0, 0x10, b"", reliable=False)
        out = {}
        for _, ib in tget(rep, 1):
            data = tget(ib, 1)
            out[tget(data, 1, 4)] = tget(data, 2)
        return out


def creds():
    src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "lib",
                            "libesp32_div", "matter_c", "include", "mtrc_attest_creds.h"), encoding="utf-8").read()
    out = {}
    for name in ("MTRC_DAC_DER", "MTRC_PAI_DER"):
        body = re.search(name + r"\[\]\s*=\s*\{(.*?)\};", src, re.S).group(1)
        out[name] = bytes(int(h, 16) for h in re.findall(r"0x([0-9a-fA-F]{2})", body))
    return out


def ecdsa_verify(pub, msg, sig):
    e = int.from_bytes(hashlib.sha256(msg).digest(), "big")
    r, s_ = int.from_bytes(sig[:32], "big"), int.from_bytes(sig[32:], "big")
    w = pow(s_, N - 2, N)
    pt = ec_add(ec_mul(e * w % N, G), ec_mul(r * w % N, pub))
    return pt is not None and pt[0] % N == r



def der_tlv(b, i):
    """-> (tag, content_start, content_end) of the DER element at i"""
    tag, ln = b[i], b[i + 1]; j = i + 2
    if ln & 0x80:
        n = ln & 0x7F; ln = int.from_bytes(b[j:j + n], "big"); j += n
    return tag, j, j + ln


def check_csr(csr):
    """CSR self-signature: ECDSA-SHA256 over CertificationRequestInfo with its own key"""
    _, s0, _ = der_tlv(csr, 0)
    _, c0, c1 = der_tlv(csr, s0)                  # CertificationRequestInfo
    cri = csr[s0:c1]
    _, a0, a1 = der_tlv(csr, c1)                  # signatureAlgorithm
    _, b0, b1 = der_tlv(csr, a1)                  # BIT STRING
    sig = csr[b0 + 1:b1]                          # skip unused-bits byte
    _, q0, _ = der_tlv(sig, 0)
    _, r0, r1 = der_tlv(sig, q0)
    _, t0, t1 = der_tlv(sig, r1)
    raw = int.from_bytes(sig[r0:r1], "big").to_bytes(32, "big") + int.from_bytes(sig[t0:t1], "big").to_bytes(32, "big")
    k = cri.find(bytes.fromhex("034200")) + 3
    return ecdsa_verify(dec(cri[k:k + 65]), cri, raw)

def im_checks(sec, att):
    ok = True
    bi = sec.read(0, 0x0028, [1, 2, 3, 0x0F])
    print(f"BasicInformation: VendorName={bi.get(1)!r} VendorID={bi.get(2)} "
          f"ProductName={bi.get(3)!r} SerialNumber={bi.get(0x0F)!r}")
    ok &= bi.get(1) == "Tasmota"
    cr = creds()
    for ct, name in ((1, "MTRC_DAC_DER"), (2, "MTRC_PAI_DER")):
        ib = sec.invoke(0, 0x003E, 0x02, [t_uint(0, ct)])
        der = tget(ib, 1, 0)
        same = der == cr[name]
        ok &= same
        print(f"CertificateChainResponse {name[5:8]}: {len(der)} B, "
              + ("identical to mtrc_attest_creds.h" if same else "DIFFERS"))
    nonce = os.urandom(32)
    ib = sec.invoke(0, 0x003E, 0x00, [tlv_bytes(0, nonce)])
    elems, sig = tget(ib, 1, 0), tget(ib, 1, 1)
    dac = cr["MTRC_DAC_DER"]
    k = dac.find(bytes.fromhex("034200")) + 3
    good = ecdsa_verify(dec(dac[k:k + 65]), elems + att, sig)
    ok &= good
    print("AttestationResponse: signature " + ("valid (DAC key, elements || challenge)" if good else "INVALID"))
    # CSRRequest -> CSRResponse {0: NOCSRElements, 1: signature}
    nonce = os.urandom(32)
    ib = sec.invoke(0, 0x003E, 0x04, [tlv_bytes(0, nonce)])
    nocsr, sig = tget(ib, 1, 0), tget(ib, 1, 1)
    (_, el), _ = tlv_tree(nocsr)
    csr, echo = tget(el, 1), tget(el, 2)
    good_dac = ecdsa_verify(dec(dac[k:k + 65]), nocsr + att, sig)
    good_csr = check_csr(csr)
    ok &= good_dac and good_csr and echo == nonce
    print(f"CSRResponse: CSR {len(csr)} B, self-signature " + ("valid" if good_csr else "INVALID")
          + ", NOCSR signature " + ("valid" if good_dac else "INVALID")
          + ", nonce " + ("echoed" if echo == nonce else "WRONG"))
    # data model of the running script (examples/matter_plug.tc: endpoint 1,
    # OnOff + ActivePower updated every second) — two reads a few seconds apart
    try:
        v1 = sec.read(1, 0x0090, [0]).get(0)
        time.sleep(3)
        v2 = sec.read(1, 0x0090, [0]).get(0)
        print(f"endpoint 1 ActivePower: {v1} -> {v2}" + ("  (script updates arrive)" if v1 != v2 else ""))
    except StopIteration:
        print("endpoint 1 ActivePower: not present (no matter_plug.tc running)")
    return ok

def main():
    args = sys.argv[1:]
    reads = []
    while "--read" in args:                     # --read ep:cluster:attr (numbers, 0x.. ok)
        i = args.index("--read")
        reads.append(tuple(int(x, 0) for x in args[i + 1].split(":")))
        del args[i:i + 2]
    ip = args[0]
    passcode = int(args[1]) if len(args) > 1 else 13572468
    M, Npt = spake_points()
    lk = Link(ip)

    t0 = time.time()
    req = b"\x15" + tlv_bytes(1, os.urandom(32)) + b"\x25\x02" + struct.pack("<H", 0x1234) + \
          b"\x24\x03\x00" + b"\x28\x04" + b"\x18"
    lk.send(0x20, req)
    resp = lk.recv(0x21)
    r = tlv_parse(resp)
    it, salt = r[4][1], r[4][2]
    print(f"PBKDFParamResponse: responder session {r[3]}, iterations {it}, salt {salt.hex()}")

    ws = hashlib.pbkdf2_hmac("sha256", struct.pack("<I", passcode), salt, it, 80)
    w0 = int.from_bytes(ws[:40], "big") % N
    w1 = int.from_bytes(ws[40:], "big") % N
    x = int.from_bytes(os.urandom(32), "big") % N
    X = ec_add(ec_mul(x, G), ec_mul(w0, M))
    lk.send(0x22, b"\x15" + tlv_bytes(1, enc(X)) + b"\x18")
    p2 = tlv_parse(lk.recv(0x23))
    Y, cB = dec(p2[1]), p2[2]
    print(f"Pake2 received after {time.time() - t0:.1f} s")

    T = ec_add(Y, ec_neg(ec_mul(w0, Npt)))
    Z, V = ec_mul(x, T), ec_mul(w1, T)
    ctx = hashlib.sha256(b"CHIP PAKE V1 Commissioning" + req + resp).digest()
    tt = b""
    for v in (ctx, b"", b"", enc(M), enc(Npt), enc(X), enc(Y), enc(Z), enc(V), w0.to_bytes(32, "big")):
        tt += struct.pack("<Q", len(v)) + v
    k = hashlib.sha256(tt).digest()
    kc = hkdf(k[:16], b"ConfirmationKeys", 32)
    cA = hmac.new(kc[:16], enc(Y), hashlib.sha256).digest()
    cB_mine = hmac.new(kc[16:], enc(X), hashlib.sha256).digest()
    if cB != cB_mine:
        print("FAIL: device cB does not match (its SPAKE2+ result differs)")
        return 1
    print("cB matches: the device's SPAKE2+ result is right")

    lk.send(0x24, b"\x15" + tlv_bytes(1, cA) + b"\x18")
    sr = lk.recv(0x40)
    gen, proto, code = struct.unpack_from("<HIH", sr)
    lk.send(0x10, b"", reliable=False)       # ack the StatusReport
    ok = gen == 0 and proto == 0 and code == 0
    print(f"StatusReport general={gen} protocol={proto} code={code} -> "
          + ("PASE SESSION ESTABLISHED" if ok else "FAILED"))
    if not ok:
        return 1
    sk = hkdf(k[16:], b"SessionKeys", 48)
    sec = Secure(lk, r[3], sk[:16], sk[16:32])
    ok = im_checks(sec, sk[32:])
    for ep, cl, at in reads:
        print(f"read ep {ep} cluster 0x{cl:04X} attr 0x{at:04X}: {sec.read(ep, cl, [at]).get(at)!r}")
    print("ALL CHECKS PASSED" if ok else "SOME CHECKS FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
