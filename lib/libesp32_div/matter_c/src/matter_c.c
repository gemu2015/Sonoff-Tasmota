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

static void pase_dispatch(const uint8_t *buf, size_t len, uint16_t src_port) {
  (void)src_port;
  mtrc_msg_header mh; mtrc_proto_header ph;
  const uint8_t *pl; size_t pll;
  if (mtrc_frame_decode(buf, len, &mh, &ph, &pl, &pll) <= 0) return;
  // PASE rides the unsecured session (id 0), Secure Channel protocol.
  if (mh.session_id != 0 || ph.protocol_id != MTRC_PROTO_SECURE_CHANNEL) return;
  g.exchange_id = ph.exchange_id;
  switch (ph.opcode) {
    case MTRC_SC_PBKDF_PARAM_REQ: pase_handle_param_req(pl, pll, &mh); break;
    // TODO P2b: MTRC_SC_PASE_PAKE1 -> Pake2 ; P2c: PASE_PAKE3 -> StatusReport
    default: break;
  }
}

void matter_loop(void) {
  if (!g.inited || !g.started) return;
  if (g.rx_pending) {
    g.rx_pending = false;
    pase_dispatch(g.rx_buf, g.rx_len, g.rx_src_port);   // crypto-heavy work here
  }
  // TODO: MRP retransmit timers + subscription report scheduler.
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
