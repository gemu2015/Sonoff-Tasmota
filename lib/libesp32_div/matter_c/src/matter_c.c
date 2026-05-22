// matter_c.c — lifecycle skeleton + stubs for the pure-C Matter library.
//
// This file compiles and links today so the host integration (gate,
// xdrv wiring, web page) can be built and exercised before the protocol
// layers exist. Protocol entry points return MATTER_ERR_NOT_IMPLEMENTED.
// Each PLAN.md phase replaces a stub with a real module:
//   Phase 1 -> mtrc_tlv      Phase 3 -> mtrc_spake2p/case/btp/...
//   Phase 4 -> mtrc_im       Phase 5 -> mtrc_ep_* (clusters)
//
// GPLv3. Inspired by Tasmota Berry Matter; implemented from the CSA spec.

#include "matter_c.h"
#include "mtrc_frame.h"
#include "mtrc_pase.h"
#include "mtrc_spake2p.h"
#include "mtrc_crypto.h"
#include "mtrc_sec.h"
#include "mtrc_im.h"
#include <string.h>
#include <stdio.h>

// ---- module state ------------------------------------------------------
static struct {
  bool             inited;
  bool             started;
  matter_port_t    port;
  matter_config_t  cfg;
  char             qr[32];     // "MT:..." (built once config is known)
  char             manual[16]; // "1234-567-8901"

  // deferred inbound packet (matter_udp_rx may be called from a network task;
  // the actual crypto-heavy processing runs in matter_loop / main loop).
  volatile bool    rx_pending;
  uint16_t         rx_src_port;
  size_t           rx_len;
  uint8_t          rx_buf[1280];

  // PASE responder session state
  uint8_t          pase_phase;        // 0 idle, 1 sent-resp, 2 sent-pake2, 3 done
  uint16_t         peer_session_id;
  uint16_t         my_session_id;
  uint16_t         exchange_id;
  uint32_t         tx_counter;        // our unsecured-session message counter
  uint8_t          init_random[32];
  uint8_t          resp_random[32];
  uint8_t          salt[16];
  uint32_t         iterations;
  uint8_t          context[32];       // SHA256(prefix || req || resp)
  uint8_t          cA_expected[32];   // prover confirmation we expect in Pake3
  uint8_t          i2r[16], r2i[16], att[16];   // PASE session keys (on success)
  bool             pase_secure;       // true once cA verified
  uint32_t         sec_tx_counter;    // our secured-session message counter
  bool             onoff;             // OnOff attribute (endpoint relay state)

  // single attribute subscription (the report engine)
  bool             sub_active;
  uint32_t         sub_id;
  uint16_t         sub_ep; uint32_t sub_cl, sub_attr;
  uint16_t         sub_max_s;         // max report interval (seconds)
  uint16_t         sub_exch;
  uint32_t         sub_last_ms;       // last report time
  uint64_t         sub_last_val;      // last reported value (change detection)
} g;

static void mlog(matter_log_level_t lvl, const char *msg) {
  if (g.port.log) g.port.log(g.port.ctx, lvl, msg);
}

// ---- lifecycle ---------------------------------------------------------
matter_err_t matter_init(const matter_port_t *port, const matter_config_t *cfg) {
  if (!port || !cfg) return MATTER_ERR_INVALID_ARG;
  // Minimum viable port: persistence + time + entropy must be present.
  if (!port->kv_get || !port->kv_set || !port->millis || !port->random_bytes)
    return MATTER_ERR_INVALID_ARG;

  memset(&g, 0, sizeof(g));
  g.port = *port;
  g.cfg  = *cfg;
  g.inited = true;

  // TODO Phase 6: build real onboarding payload (Base38 + Verhoeff + TLV).
  g.qr[0] = '\0';
  g.manual[0] = '\0';

  mlog(MATTER_LOG_INFO, "matter_c init (stub)");
  return MATTER_OK;
}

#define MTRC_COMMISSION_PORT 5540   // Matter operational/commissioning UDP port

matter_err_t matter_start(void) {
  if (!g.inited)   return MATTER_ERR_NOT_INIT;
  if (g.started)   return MATTER_OK;

  // Publish the commissionable node over mDNS (_matterc._udp) so an
  // on-network commissioner (chip-tool `pairing onnetwork`) can discover us.
  // TXT keys per Matter Core Spec §4.3.1: D=discriminator, CM=commissioning
  // mode, VP=vendor+product. (UDP listener + PASE handling come next.)
  if (g.port.mdns_publish) {
    static char txt_d[16], txt_cm[8], txt_vp[24];
    snprintf(txt_d,  sizeof(txt_d),  "D=%u", (unsigned)g.cfg.discriminator);
    snprintf(txt_cm, sizeof(txt_cm), "CM=1");   // 1 = in commissioning mode
    snprintf(txt_vp, sizeof(txt_vp), "VP=%u+%u", (unsigned)g.cfg.vendor_id,
             (unsigned)g.cfg.product_id);
    const char *txt[] = { txt_d, txt_cm, txt_vp };

    // 64-bit commissioning instance name as 16 uppercase hex (Matter spec).
    uint8_t inst[8] = {0};
    if (g.port.random_bytes) g.port.random_bytes(g.port.ctx, inst, 8);
    char instance[17];
    for (int i = 0; i < 8; i++) snprintf(instance + 2*i, 3, "%02X", inst[i]);

    matter_err_t e = g.port.mdns_publish(g.port.ctx, "matterc", instance,
                                         MTRC_COMMISSION_PORT, txt, 3);
    mlog(e == MATTER_OK ? MATTER_LOG_INFO : MATTER_LOG_ERROR,
         e == MATTER_OK ? "matter_c: commissionable mDNS published (_matterc._udp)"
                        : "matter_c: mDNS publish failed");
  } else {
    mlog(MATTER_LOG_INFO, "matter_c start — no mdns_publish port; not discoverable");
  }

  g.started = true;
  // PASE-over-UDP commissioning is the next milestone; discovery is live.
  return MATTER_OK;
}

matter_err_t matter_stop(void) {
  if (!g.inited) return MATTER_ERR_NOT_INIT;
  g.started = false;
  mlog(MATTER_LOG_INFO, "matter_c stop (stub)");
  return MATTER_OK;
}

matter_err_t matter_factory_reset(void) {
  if (!g.inited) return MATTER_ERR_NOT_INIT;
  // TODO Phase 3: enumerate + delete fabric/session keys via port.kv_del.
  mlog(MATTER_LOG_INFO, "matter_c factory reset (stub)");
  return MATTER_ERR_NOT_IMPLEMENTED;
}

// ---- PASE responder ----------------------------------------------------
// Send a Secure Channel message on the unsecured session (id 0). The
// response always reflects the initiator's exchange and (optionally) acks.
static void pase_send(uint8_t opcode, const uint8_t *payload, size_t plen,
                      bool has_ack, uint32_t ack_counter, bool reliable) {
  mtrc_msg_header mh; memset(&mh, 0, sizeof(mh));
  mh.session_id = 0; mh.session_type = 0; mh.msg_counter = ++g.tx_counter;
  mtrc_proto_header ph; memset(&ph, 0, sizeof(ph));
  ph.initiator   = false;          // we are the responder
  ph.ack         = has_ack;
  ph.ack_counter = ack_counter;
  ph.reliability = reliable;
  ph.opcode      = opcode;
  ph.exchange_id = g.exchange_id;
  ph.protocol_id = MTRC_PROTO_SECURE_CHANNEL;
  uint8_t out[1280];
  int n = mtrc_frame_encode(out, sizeof(out), &mh, &ph, payload, plen);
  if (n > 0 && g.port.udp_send)
    g.port.udp_send(g.port.ctx, NULL, 0, out, (size_t)n);
}

// PBKDFParamRequest -> PBKDFParamResponse (we choose salt + iterations).
static void pase_handle_param_req(const uint8_t *payload, size_t plen,
                                  const mtrc_msg_header *mh) {
  mtrc_pase_param_req req;
  if (!mtrc_pase_decode_param_req(payload, plen, &req)) return;
  g.peer_session_id = req.initiator_session_id;
  memcpy(g.init_random, req.initiator_random, 32);

  g.port.random_bytes(g.port.ctx, g.resp_random, 32);
  g.port.random_bytes(g.port.ctx, g.salt, 16);
  uint16_t sid = 0;
  g.port.random_bytes(g.port.ctx, (uint8_t *)&sid, 2);
  g.my_session_id = sid ? sid : 1;
  g.iterations = 1000;

  mtrc_pase_param_resp resp; memset(&resp, 0, sizeof(resp));
  memcpy(resp.initiator_random, req.initiator_random, 32);
  memcpy(resp.responder_random, g.resp_random, 32);
  resp.responder_session_id = g.my_session_id;
  resp.iterations = g.iterations;
  memcpy(resp.salt, g.salt, 16); resp.salt_len = 16;

  uint8_t rp[160];
  int rn = mtrc_pase_encode_param_resp(rp, sizeof(rp), &resp);
  if (rn < 0) return;

  // SPAKE2+ transcript context = SHA256(prefix || req || resp), saved for Pake.
  mtrc_pase_context(payload, plen, rp, (size_t)rn, g.context);

  pase_send(MTRC_SC_PBKDF_PARAM_RSP, rp, (size_t)rn, true, mh->msg_counter, true);
  g.pase_phase = 1;
  mlog(MATTER_LOG_INFO, "PASE: PBKDFParamResponse sent");
}

// Pake1 (pA) -> Pake2 (pB, cB). Verifier-side SPAKE2+ (heavy: PBKDF2 + EC).
static void pase_handle_pake1(const uint8_t *payload, size_t plen,
                              const mtrc_msg_header *mh) {
  uint8_t pA[65];
  if (!mtrc_pase_decode_pake1(payload, plen, pA)) return;

  uint8_t w0[32], w1[32], L[65], y[32], pB[65], Z[65], V[65];
  if (!mtrc_pase_derive_w0w1(g.cfg.passcode, g.salt, 16, g.iterations, w0, w1)) return;
  if (!mtrc_ec_mulgen(L, w1, 32)) return;                      // L = w1*G
  g.port.random_bytes(g.port.ctx, y, 32);
  if (!mtrc_spake2p_verifier_Y(w0, y, pB)) return;             // pB = y*G + w0*N
  if (!mtrc_spake2p_verifier_ZV(w0, y, pA, L, Z, V)) return;   // Z,V from pA,L

  mtrc_pase_keys_t k;
  if (!mtrc_pase_keys(g.context, pA, pB, Z, V, w0, &k)) return;
  memcpy(g.cA_expected, k.cA, 32);                             // expect this in Pake3
  memcpy(g.i2r, k.i2r, 16); memcpy(g.r2i, k.r2i, 16); memcpy(g.att, k.att, 16);

  uint8_t out[160];
  int n = mtrc_pase_encode_pake2(out, sizeof(out), pB, k.cB);  // send pB + cB
  if (n < 0) return;
  pase_send(MTRC_SC_PASE_PAKE2, out, (size_t)n, true, mh->msg_counter, true);
  g.pase_phase = 2;
  mlog(MATTER_LOG_INFO, "PASE: Pake2 sent (SPAKE2+ verifier)");
}

// Pake3 (cA) -> verify, then StatusReport. On success the PASE session keys
// are live (g.i2r / g.r2i / g.att).
static void pase_handle_pake3(const uint8_t *payload, size_t plen,
                              const mtrc_msg_header *mh) {
  uint8_t cA[32];
  if (!mtrc_pase_decode_pake3(payload, plen, cA)) return;
  uint8_t diff = 0;
  for (int i = 0; i < 32; i++) diff |= (uint8_t)(cA[i] ^ g.cA_expected[i]);

  // Secure Channel StatusReport: GeneralCode(2) | ProtocolId(4) | Code(2), LE.
  uint8_t sr[8]; memset(sr, 0, 8);
  if (diff == 0) {
    // GeneralCode=0 (Success), ProtocolId=0 (SecureChannel),
    // Code=0 (SessionEstablishmentSuccess)
    g.pase_secure = true; g.pase_phase = 3;
    pase_send(MTRC_SC_STATUS_REPORT, sr, 8, true, mh->msg_counter, true);
    mlog(MATTER_LOG_INFO, "PASE: cA verified -> SESSION ESTABLISHED (StatusReport success)");
  } else {
    sr[0] = 0x01;   // GeneralCode = 1 (Failure)
    pase_send(MTRC_SC_STATUS_REPORT, sr, 8, true, mh->msg_counter, true);
    g.pase_phase = 0;
    mlog(MATTER_LOG_ERROR, "PASE: cA MISMATCH -> StatusReport failure");
  }
}

// Send an encrypted message back to the commissioner on the secured PASE
// session: R2I key, our session-id assigned to the peer, our secured counter,
// acking the inbound message.
static void secured_send(uint8_t opcode, uint16_t protocol_id,
                         const uint8_t *payload, size_t plen,
                         uint16_t exch, bool has_ack, uint32_t ack_counter) {
  mtrc_msg_header mh; memset(&mh, 0, sizeof(mh));
  mh.session_id = g.peer_session_id; mh.session_type = 0;
  mh.msg_counter = ++g.sec_tx_counter;
  mtrc_proto_header ph; memset(&ph, 0, sizeof(ph));
  ph.initiator = false; ph.ack = has_ack; ph.ack_counter = ack_counter;
  ph.reliability = true; ph.opcode = opcode; ph.exchange_id = exch;
  ph.protocol_id = protocol_id;
  static uint8_t out[1280];
  int n = mtrc_sec_encode(out, sizeof(out), &mh, &ph, payload, plen, g.r2i);
  if (n > 0 && g.port.udp_send)
    g.port.udp_send(g.port.ctx, NULL, 0, out, (size_t)n);
}

// Live value of an attribute: host's on_attr_read, else cache/config.
static uint64_t attr_value(uint16_t ep, uint32_t cl, uint32_t attr) {
  uint64_t v = 0;
  if (g.port.on_attr_read &&
      g.port.on_attr_read(g.port.ctx, ep, cl, attr, &v) == MATTER_OK) return v;
  if      (cl == 0x0006 && attr == 0x0000) v = g.onoff ? 1 : 0;
  else if (cl == 0x0028 && attr == 0x0002) v = g.cfg.vendor_id;
  else if (cl == 0x0028 && attr == 0x0004) v = g.cfg.product_id;
  return v;
}

// Handle a decrypted IM InvokeRequest. P3b.1 answers the General
// Commissioning commands ({errorCode, debugText}); anything else gets an
// UNSUPPORTED_COMMAND status.
static void im_handle_invoke(const uint8_t *payload, size_t plen,
                             uint16_t exch, uint32_t ack) {
  uint16_t ep; uint32_t cl, cmd;
  if (!mtrc_im_parse_first_command(payload, plen, &ep, &cl, &cmd)) return;
  char m[80];
  snprintf(m, sizeof(m), "IM Invoke ep=%u cluster=0x%04X cmd=0x%02X",
           (unsigned)ep, (unsigned)cl, (unsigned)cmd);
  mlog(MATTER_LOG_INFO, m);

  static uint8_t resp[256];
  int n = -1;
  if (cl == 0x0030) {                 // General Commissioning
    uint32_t rc = 0xFFFFFFFF;
    if      (cmd == 0x00) rc = 0x01;  // ArmFailSafe -> ArmFailSafeResponse
    else if (cmd == 0x02) rc = 0x03;  // SetRegulatoryConfig -> Response
    else if (cmd == 0x04) rc = 0x05;  // CommissioningComplete -> Response
    if (rc != 0xFFFFFFFF)
      n = mtrc_im_build_cmd_response_u8(resp, sizeof(resp), ep, cl, rc, 0); // errorCode OK
  } else if (cl == 0x0006 && cmd <= 0x02) {   // OnOff: Off(0)/On(1)/Toggle(2)
    if (g.port.on_attr_write) {
      uint8_t action = (uint8_t)cmd;          // maps 1:1 to Tasmota POWER_*
      g.port.on_attr_write(g.port.ctx, ep, 0x0006, 0x0000, &action, 1);
    }
    g.onoff = (cmd == 0x02) ? !g.onoff : (cmd == 0x01);
    n = mtrc_im_build_status(resp, sizeof(resp), ep, cl, cmd, 0x00);   // SUCCESS
  }
  if (n < 0)
    n = mtrc_im_build_status(resp, sizeof(resp), ep, cl, cmd, 0x81); // UNSUPPORTED_COMMAND

  if (n > 0) {
    secured_send(MTRC_IM_INVOKE_RESPONSE, MTRC_PROTO_IM, resp, (size_t)n, exch, true, ack);
    mlog(MATTER_LOG_INFO, "IM InvokeResponse sent");
  }
}

// Handle a decrypted IM ReadRequest: report the requested attribute. P3/B1
// supports OnOff (0x0006/0x0000 -> relay state) and a couple of Basic
// Information attrs; everything else reports 0.
static void im_handle_read(const uint8_t *payload, size_t plen,
                           uint16_t exch, uint32_t ack) {
  uint16_t ep; uint32_t cl, attr;
  if (!mtrc_im_parse_first_attribute(payload, plen, &ep, &cl, &attr)) return;
  char m[80];
  snprintf(m, sizeof(m), "IM Read ep=%u cluster=0x%04X attr=0x%04X",
           (unsigned)ep, (unsigned)cl, (unsigned)attr);
  mlog(MATTER_LOG_INFO, m);

  static uint8_t resp[160];
  int n = mtrc_im_build_report_uint(resp, sizeof(resp), 0, ep, cl, attr,
                                    attr_value(ep, cl, attr));
  if (n > 0) {
    secured_send(MTRC_IM_REPORT_DATA, MTRC_PROTO_IM, resp, (size_t)n, exch, true, ack);
    mlog(MATTER_LOG_INFO, "IM ReportData sent");
  }
}

// Handle a SubscribeRequest: register a subscription, send a priming
// ReportData + SubscribeResponse. Periodic/changed reports come from
// matter_loop. (Single subscription supported.)
static void im_handle_subscribe(const uint8_t *payload, size_t plen,
                                uint16_t exch, uint32_t ack) {
  uint16_t ep, maxc; uint32_t cl, attr;
  if (!mtrc_im_parse_subscribe(payload, plen, &ep, &cl, &attr, &maxc)) return;
  g.sub_active = true;
  g.sub_id = (g.sub_id ? g.sub_id : 1) + 1;
  g.sub_ep = ep; g.sub_cl = cl; g.sub_attr = attr;
  g.sub_max_s = (maxc == 0 || maxc > 60) ? 30 : maxc;   // clamp
  g.sub_exch = exch;
  g.sub_last_val = attr_value(ep, cl, attr);
  g.sub_last_ms = g.port.millis(g.port.ctx);

  char m[80];
  snprintf(m, sizeof(m), "IM Subscribe ep=%u cl=0x%04X attr=0x%04X max=%us id=%u",
           (unsigned)ep,(unsigned)cl,(unsigned)attr,(unsigned)g.sub_max_s,(unsigned)g.sub_id);
  mlog(MATTER_LOG_INFO, m);

  static uint8_t rep[160];
  int n = mtrc_im_build_report_uint(rep, sizeof(rep), g.sub_id, ep, cl, attr, g.sub_last_val);
  if (n > 0) secured_send(MTRC_IM_REPORT_DATA, MTRC_PROTO_IM, rep, (size_t)n, exch, true, ack);
  n = mtrc_im_build_subscribe_response(rep, sizeof(rep), g.sub_id, g.sub_max_s);
  if (n > 0) secured_send(MTRC_IM_SUBSCRIBE_RESPONSE, MTRC_PROTO_IM, rep, (size_t)n, exch, false, 0);
  mlog(MATTER_LOG_INFO, "IM SubscribeResponse sent (priming report + subscribe)");
}

// A message arrived on the established PASE secure session: decrypt with the
// I2R key, parse the inner protocol header, dispatch the IM.
static void secured_dispatch(const uint8_t *buf, size_t len) {
  mtrc_msg_header mh; mtrc_proto_header ph;
  const uint8_t *ipl; size_t ipll;
  static uint8_t pt[1280];
  if (!mtrc_sec_decode(buf, len, g.i2r, &mh, &ph, pt, sizeof(pt), &ipl, &ipll)) {
    mlog(MATTER_LOG_ERROR, "secured rx: MIC/decrypt failed");
    return;
  }
  if (ph.protocol_id == MTRC_PROTO_IM && ph.opcode == MTRC_IM_INVOKE_REQUEST) {
    im_handle_invoke(ipl, ipll, ph.exchange_id, mh.msg_counter);
  } else if (ph.protocol_id == MTRC_PROTO_IM && ph.opcode == MTRC_IM_READ_REQUEST) {
    im_handle_read(ipl, ipll, ph.exchange_id, mh.msg_counter);
  } else if (ph.protocol_id == MTRC_PROTO_IM && ph.opcode == MTRC_IM_SUBSCRIBE_REQUEST) {
    im_handle_subscribe(ipl, ipll, ph.exchange_id, mh.msg_counter);
  } else {
    char m[80];
    snprintf(m, sizeof(m), "secured rx proto=0x%04X op=0x%02X (unhandled)",
             (unsigned)ph.protocol_id, (unsigned)ph.opcode);
    mlog(MATTER_LOG_INFO, m);
  }
}

static void pase_dispatch(const uint8_t *buf, size_t len, uint16_t src_port) {
  (void)src_port;
  // Peek the message header to route by session id.
  mtrc_msg_header mh0;
  if (mtrc_frame_decode_msg_header(buf, len, &mh0) < 0) return;

  if (mh0.session_id != 0) {
    // Secured session traffic (post-PASE commissioning IM).
    if (g.pase_secure && mh0.session_id == g.my_session_id)
      secured_dispatch(buf, len);
    return;
  }

  // Unsecured session (id 0): PASE over Secure Channel.
  mtrc_msg_header mh; mtrc_proto_header ph;
  const uint8_t *pl; size_t pll;
  if (mtrc_frame_decode(buf, len, &mh, &ph, &pl, &pll) <= 0) return;
  if (ph.protocol_id != MTRC_PROTO_SECURE_CHANNEL) return;
  g.exchange_id = ph.exchange_id;
  switch (ph.opcode) {
    case MTRC_SC_PBKDF_PARAM_REQ: pase_handle_param_req(pl, pll, &mh); break;
    case MTRC_SC_PASE_PAKE1:      pase_handle_pake1(pl, pll, &mh);     break;
    case MTRC_SC_PASE_PAKE3:      pase_handle_pake3(pl, pll, &mh);     break;
    default: break;
  }
}

void matter_loop(void) {
  if (!g.inited || !g.started) return;
  if (g.rx_pending) {
    g.rx_pending = false;
    pase_dispatch(g.rx_buf, g.rx_len, g.rx_src_port);   // crypto-heavy work here
  }
  // Subscription report engine: send a ReportData when the value changed or
  // the max interval elapsed.
  if (g.sub_active && g.pase_secure) {
    uint32_t now = g.port.millis(g.port.ctx);
    uint64_t v = attr_value(g.sub_ep, g.sub_cl, g.sub_attr);
    if (v != g.sub_last_val || (now - g.sub_last_ms) >= (uint32_t)g.sub_max_s * 1000u) {
      g.sub_last_val = v; g.sub_last_ms = now;
      static uint8_t rep[160];
      int n = mtrc_im_build_report_uint(rep, sizeof(rep), g.sub_id,
                                        g.sub_ep, g.sub_cl, g.sub_attr, v);
      if (n > 0)
        secured_send(MTRC_IM_REPORT_DATA, MTRC_PROTO_IM, rep, (size_t)n, g.sub_exch, false, 0);
    }
  }
}

// ---- inbound transport pumps ------------------------------------------
// May run in a network task — only copy + flag; processing is in matter_loop.
void matter_udp_rx(const uint8_t src_ip6[16], uint16_t src_port,
                   const void *buf, size_t len) {
  (void)src_ip6;
  if (!g.inited || g.rx_pending) return;        // sequential; MRP retransmits
  if (len == 0 || len > sizeof(g.rx_buf)) return;
  memcpy(g.rx_buf, buf, len);
  g.rx_len = len; g.rx_src_port = src_port;
  g.rx_pending = true;
}

void matter_ble_rx(const void *buf, size_t len) {
  (void)buf; (void)len;
  // TODO Phase 3: BTP reassembly -> PASE/commissioning message handler.
}

// ---- endpoints / attributes -------------------------------------------
int matter_add_endpoint(uint32_t device_type_id) {
  if (!g.inited) return MATTER_ERR_NOT_INIT;
  (void)device_type_id;
  // TODO Phase 5: allocate endpoint, attach the device-type's cluster set.
  return MATTER_ERR_NOT_IMPLEMENTED;
}

matter_err_t matter_set_attr(uint16_t endpoint, uint32_t cluster,
                             uint32_t attr, const uint8_t *tlv, size_t tlv_len) {
  if (!g.inited) return MATTER_ERR_NOT_INIT;
  (void)endpoint; (void)cluster; (void)attr; (void)tlv; (void)tlv_len;
  // TODO Phase 4: store value + emit subscription reports.
  return MATTER_ERR_NOT_IMPLEMENTED;
}

// ---- onboarding + introspection ---------------------------------------
const char *matter_qr_uri(void)      { return g.qr; }      // "" until Phase 6
const char *matter_manual_code(void) { return g.manual; }  // "" until Phase 6
const char *matter_version(void)     { return MATTER_C_VERSION_STR; }
bool        matter_is_commissioned(void) { return false; } // TODO Phase 3
