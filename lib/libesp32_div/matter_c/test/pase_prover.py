#!/usr/bin/env python3
# Full SPAKE2+ PASE prover (acts as chip-tool) to drive the matter_c device
# responder through the complete handshake and verify StatusReport success.
import socket, sys, struct, hashlib, hmac
from ecdsa.ellipticcurve import Point
from ecdsa.curves import NIST256p

DEV = sys.argv[1] if len(sys.argv) > 1 else "192.168.188.122"
PASSCODE = 20202021

curve = NIST256p.curve
G = NIST256p.generator
order = NIST256p.order
# SPAKE2+ M, N for P-256 (RFC 9383), uncompressed coords
M = Point(curve, 0x886e2f97ace46e55ba9dd7242579f2993b64e16ef3dcab95afd497333d8fa12f,
                 0x5ff355163e43ce224e0b0e65ff02ac8e5c7be09419c785e0ca547d55a12e2d20)
N = Point(curve, 0xd8bbd6c639c62937b04d997f38c3770719c629d7014d49a24b4f98baa1292b49,
                 0x07d60aa6bfade45008a636337f5168c64d9bd36034808cd564490b1e656edbe7)

def pt_bytes(P):
    return b'\x04' + P.x().to_bytes(32,'big') + P.y().to_bytes(32,'big')
def pt_from(b):
    return Point(curve, int.from_bytes(b[1:33],'big'), int.from_bytes(b[33:65],'big'))
def hkdf(ikm, salt, info, n):
    if not salt: salt = b'\x00'*32
    prk = hmac.new(salt, ikm, hashlib.sha256).digest()
    t=b''; okm=b''; i=1
    while len(okm)<n:
        t=hmac.new(prk, t+info+bytes([i]), hashlib.sha256).digest(); okm+=t; i+=1
    return okm[:n]
def tt_add(buf, v): buf += struct.pack('<Q', len(v)) + v

# ---- transport ----
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(6)
my_ctr = 0
def send(op, payload, ack=None, exch=1):
    global my_ctr; my_ctr += 1
    xf = 0x01 | 0x04 | (0x02 if ack is not None else 0)   # I + R (+A)
    ph = bytes([xf, op]) + struct.pack('<H', exch) + struct.pack('<H', 0)
    if ack is not None: ph += struct.pack('<I', ack)
    frame = b'\x00' + struct.pack('<H',0) + b'\x00' + struct.pack('<I',my_ctr) + ph + payload
    s.sendto(frame, (DEV, 5540))
def recv():
    r,_ = s.recvfrom(2048)
    off=8; xf=r[off]; op=r[off+1]; o=off+6
    if xf & 0x10: o+=2
    ackc = None
    if xf & 0x02: ackc=struct.unpack('<I', r[o:o+4])[0]; o+=4
    ctr = struct.unpack('<I', r[4:8])[0]
    return op, r[o:], ctr   # opcode, payload, device msg counter

# ---- 1) PBKDFParamRequest ----
req_payload = (b'\x15' + b'\x30\x01\x20' + bytes(range(32)) +
               b'\x25\x02' + struct.pack('<H',0x1111) + b'\x24\x03\x00' + b'\x28\x04' + b'\x18')
send(0x20, req_payload)
op, resp_payload, dctr = recv()
assert op == 0x21, "expected PBKDFParamResponse, got 0x%02X" % op
print("1) PBKDFParamResponse OK (%dB)" % len(resp_payload))

# parse salt + iterations from resp TLV (walk for ctx4 struct -> ctx1 iter, ctx2 salt)
def find_pbkdf(tlv):
    # crude: locate iterations (ctx1 u16/u32 inside pbkdf_parameters) and salt (ctx2 octstr16)
    i=0; it=None; salt=None
    while i < len(tlv):
        c=tlv[i]
        # ctx1 u16 0x25 inside params: iterations
        if c==0x25 and tlv[i+1]==0x01: it=struct.unpack('<H',tlv[i+2:i+4])[0]; i+=4; continue
        if c==0x30 and tlv[i+1]==0x02 and tlv[i+2]==0x10: salt=tlv[i+3:i+19]; i+=19; continue
        i+=1
    return it, salt
iters, salt = find_pbkdf(resp_payload)
assert salt and iters, "could not parse salt/iterations"
print("   iterations=%d salt=%s" % (iters, salt.hex()))

# ---- w0,w1 ----
ws = hashlib.pbkdf2_hmac('sha256', PASSCODE.to_bytes(4,'little'), salt, iters, 80)
w0 = int.from_bytes(ws[:40],'big') % order
w1 = int.from_bytes(ws[40:80],'big') % order

# ---- 2) Pake1: X = x*G + w0*M ----
x = 0x1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef % order
X = x*G + w0*M
pA = pt_bytes(X)
send(0x22, b'\x15\x30\x01\x41' + pA + b'\x18', ack=dctr)   # struct{ctx1 octstr65 pA}
op, p2, dctr = recv()
assert op == 0x23, "expected Pake2, got 0x%02X" % op
# parse pB (ctx1 octstr65) + cB (ctx2 octstr32)
assert p2[0]==0x15 and p2[1]==0x30 and p2[2]==0x01 and p2[3]==0x41
pB = p2[4:69]; assert p2[69]==0x30 and p2[70]==0x02 and p2[71]==0x20
cB = p2[72:104]
print("2) Pake2 OK: pB(65) + cB(32)")

# ---- prover Z,V + key schedule ----
Y = pt_from(pB)
T = Y + ((order - w0) % order)*N         # Y - w0*N
Z = pt_bytes(x*T); V = pt_bytes(w1*T)
context = hashlib.sha256(b"CHIP PAKE V1 Commissioning" + req_payload + resp_payload).digest()
tt=bytearray()
for v in (context, b'', b'', pt_bytes(M), pt_bytes(N), pA, pB, Z, V, w0.to_bytes(32,'big')):
    tt_add(tt, v)
K_main = hashlib.sha256(bytes(tt)).digest()
Ka, Ke = K_main[:16], K_main[16:]
kcab = hkdf(Ka, b'', b'ConfirmationKeys', 32)
KcA, KcB = kcab[:16], kcab[16:]
cB_check = hmac.new(KcB, pA, hashlib.sha256).digest()
cA = hmac.new(KcA, pB, hashlib.sha256).digest()
print("   device cB %s prover-recomputed cB" % ("==" if cB==cB_check else "!=  MISMATCH"))
assert cB == cB_check, "cB mismatch -> device SPAKE2+ wrong"

# ---- 3) Pake3 (cA) -> StatusReport ----
send(0x24, b'\x15\x30\x01\x20' + cA + b'\x18', ack=dctr)
op, sr, dctr = recv()
gen = struct.unpack('<H', sr[0:2])[0] if len(sr)>=2 else 0xFFFF
ok = (op==0x40 and gen==0)
print("3) StatusReport op=0x%02X GeneralCode=%d (%s)" % (op, gen, "SUCCESS" if ok else "FAIL"))
print("==> PASE handshake %s" % ("PASS — session established" if ok else "FAILED"))

# ---- 4) Commissioning IM over the secured session: ArmFailSafe round-trip ----
if ok and len(sys.argv) > 2 and sys.argv[2] in ("secured", "commission"):
    from cryptography.hazmat.primitives.ciphers.aead import AESCCM
    rsid = struct.unpack('<H', resp_payload[ resp_payload.find(b'\x25\x03')+2 :
                                             resp_payload.find(b'\x25\x03')+4 ])[0]
    sek = hkdf(Ke, b'', b'SessionKeys', 48)
    I2R, R2I = sek[:16], sek[16:32]
    init_sid = 0x1111            # we advertised this as initiatorSessionId

    def u16(t,v): return bytes([0x25,t])+struct.pack('<H',v)
    def u32(t,v): return bytes([0x26,t])+struct.pack('<I',v)
    def u64(t,v): return bytes([0x27,t])+struct.pack('<Q',v)
    def u8(t,v):  return bytes([0x24,t,v&0xff])
    def b0(t):    return bytes([0x28,t])      # bool false
    # ArmFailSafe (cluster 0x30 cmd 0x00) InvokeRequest
    path  = bytes([0x37,0x00]) + u16(0,0) + u32(1,0x30) + u32(2,0x00) + b'\x18'  # CommandPath list
    flds  = bytes([0x35,0x01]) + u16(0,60) + u64(1,0)   + b'\x18'               # {expiry,breadcrumb}
    cdata = b'\x15' + path + flds + b'\x18'                                     # CommandDataIB
    inv   = bytes([0x36,0x02]) + cdata + b'\x18'                                # InvokeRequests array
    msg   = b'\x15' + b0(0) + b0(1) + inv + u8(0xFF,1) + b'\x18'                # InvokeRequestMessage

    sctr = 1
    proto = bytes([0x05, 0x08]) + struct.pack('<H',2) + struct.pack('<H',0x0001)  # IM InvokeRequest, exch 2
    mhdr  = b'\x00' + struct.pack('<H', rsid) + b'\x00' + struct.pack('<I', sctr)
    nonce = b'\x00' + struct.pack('<I', sctr) + struct.pack('<Q', 0)
    ct = AESCCM(I2R, tag_length=16).encrypt(nonce, proto+msg, mhdr)
    s.sendto(mhdr + ct, (DEV, 5540))
    print("4) sent ArmFailSafe InvokeRequest (encrypted I2R)")
    try:
        r,_ = s.recvfrom(2048)
        sf = r[3]; dctr2 = struct.unpack('<I', r[4:8])[0]
        non = bytes([sf]) + struct.pack('<I',dctr2) + struct.pack('<Q',0)
        pt = AESCCM(R2I, tag_length=16).decrypt(non, r[8:], r[0:8])
        xf=pt[0]; op=pt[1]; o=6
        if xf&0x10:o+=2
        if xf&0x02:o+=4
        body = pt[o:]
        # device encodes path ids minimal-width (u8): cluster 0x30 -> 24 01 30,
        # ArmFailSafeResponse cmd 0x01 -> 24 02 01.
        has_resp = (b'\x24\x01\x30' in body) and (b'\x24\x02\x01' in body)
        print("   decrypted InvokeResponse op=0x%02X  ArmFailSafeResponse=%s"
              % (op, "yes" if (op==0x09 and has_resp) else "NO"))
        print("\n==> COMMISSIONING IM %s" %
              ("PASS — ArmFailSafe answered over secure session"
               if (op==0x09 and has_resp) else "FAILED"))
    except socket.timeout:
        print("   NO RESPONSE (timeout)")
s.close()
