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

void matter_loop(void) {
  if (!g.inited || !g.started) return;
  // TODO Phase 2: MRP retransmit timers + subscription report scheduler.
}

// ---- inbound transport pumps ------------------------------------------
void matter_udp_rx(const uint8_t src_ip6[16], uint16_t src_port,
                   const void *buf, size_t len) {
  (void)src_ip6;
  if (!g.inited) return;

  // Decode the (unsecured-session) message + protocol header and log it.
  // PASE starts on the unsecured session (id 0) in plaintext, so the frame
  // decoder applies directly. Dispatch to the PASE responder is the next step.
  mtrc_msg_header mh; mtrc_proto_header ph;
  const uint8_t *pl; size_t pll;
  char m[112];
  if (mtrc_frame_decode((const uint8_t *)buf, len, &mh, &ph, &pl, &pll) > 0) {
    snprintf(m, sizeof(m),
             "rx %u B from :%u  sess=%u proto=0x%04X op=0x%02X exch=%u R=%d pl=%u",
             (unsigned)len, (unsigned)src_port, (unsigned)mh.session_id,
             (unsigned)ph.protocol_id, (unsigned)ph.opcode,
             (unsigned)ph.exchange_id, ph.reliability ? 1 : 0, (unsigned)pll);
  } else {
    snprintf(m, sizeof(m), "rx %u B from :%u  (frame decode failed)",
             (unsigned)len, (unsigned)src_port);
  }
  mlog(MATTER_LOG_INFO, m);
  // TODO P2: dispatch unsecured Secure Channel msgs to the PASE responder.
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
