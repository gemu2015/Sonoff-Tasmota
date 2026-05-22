// mtrc_im.c — Matter Interaction Model subset. See mtrc_im.h. GPLv3.

#include "mtrc_im.h"
#include "mtrc_tlv.h"

int mtrc_im_parse_first_command(const uint8_t *buf, size_t len,
                                uint16_t *endpoint, uint32_t *cluster,
                                uint32_t *command) {
  // The only TLV list in an InvokeRequest is the CommandPath
  // {0:endpoint, 1:cluster, 2:command}. Scan for it.
  mtrc_tlv_reader r; mtrc_tlv_reader_init(&r, buf, len);
  mtrc_tlv_elem e;
  while (mtrc_tlv_read(&r, &e)) {
    if (e.type != MTRC_TLV_LIST) continue;
    uint16_t ep = 0; uint32_t cl = 0, cmd = 0; int have = 0;
    mtrc_tlv_elem ie;
    while (mtrc_tlv_read(&r, &ie) && ie.type != MTRC_TLV_END) {
      if (ie.tag.ctrl != MTRC_TLV_TAG_CONTEXT) continue;
      if      (ie.tag.number == 0) { ep  = (uint16_t)ie.u; have |= 1; }
      else if (ie.tag.number == 1) { cl  = (uint32_t)ie.u; have |= 2; }
      else if (ie.tag.number == 2) { cmd = (uint32_t)ie.u; have |= 4; }
    }
    if (have == 7) { *endpoint = ep; *cluster = cl; *command = cmd; return 1; }
  }
  return 0;
}

int mtrc_im_build_cmd_response_u8(uint8_t *out, size_t cap,
                                  uint16_t endpoint, uint32_t cluster,
                                  uint32_t resp_command, uint8_t field0) {
  mtrc_tlv_writer w; mtrc_tlv_writer_init(&w, out, cap);
  mtrc_tlv_start_struct(&w, mtrc_tlv_anon());          // InvokeResponseMessage
  mtrc_tlv_put_bool (&w, mtrc_tlv_ctx(0), false);      // suppressResponse
  mtrc_tlv_start_array(&w, mtrc_tlv_ctx(1));           // InvokeResponses
  mtrc_tlv_start_struct(&w, mtrc_tlv_anon());          //  InvokeResponseIB
  mtrc_tlv_start_struct(&w, mtrc_tlv_ctx(0));          //   command = CommandDataIB
  mtrc_tlv_start_list(&w, mtrc_tlv_ctx(0));            //    CommandPath (list)
  mtrc_tlv_put_uint(&w, mtrc_tlv_ctx(0), endpoint);
  mtrc_tlv_put_uint(&w, mtrc_tlv_ctx(1), cluster);
  mtrc_tlv_put_uint(&w, mtrc_tlv_ctx(2), resp_command);
  mtrc_tlv_end_container(&w);                          //    end CommandPath
  mtrc_tlv_start_struct(&w, mtrc_tlv_ctx(1));          //    CommandFields
  mtrc_tlv_put_uint(&w, mtrc_tlv_ctx(0), field0);      //     0: errorCode
  mtrc_tlv_put_utf8(&w, mtrc_tlv_ctx(1), "", 0);       //     1: debugText ""
  mtrc_tlv_end_container(&w);                          //    end CommandFields
  mtrc_tlv_end_container(&w);                          //   end CommandDataIB
  mtrc_tlv_end_container(&w);                          //  end InvokeResponseIB
  mtrc_tlv_end_container(&w);                          // end InvokeResponses
  mtrc_tlv_put_uint(&w, mtrc_tlv_ctx(0xFF), 1);        // interactionModelRevision
  mtrc_tlv_end_container(&w);                          // end message
  return mtrc_tlv_writer_ok(&w) ? (int)mtrc_tlv_writer_len(&w) : -1;
}

int mtrc_im_build_status(uint8_t *out, size_t cap,
                         uint16_t endpoint, uint32_t cluster, uint32_t command,
                         uint8_t status) {
  mtrc_tlv_writer w; mtrc_tlv_writer_init(&w, out, cap);
  mtrc_tlv_start_struct(&w, mtrc_tlv_anon());          // InvokeResponseMessage
  mtrc_tlv_put_bool (&w, mtrc_tlv_ctx(0), false);      // suppressResponse
  mtrc_tlv_start_array(&w, mtrc_tlv_ctx(1));           // InvokeResponses
  mtrc_tlv_start_struct(&w, mtrc_tlv_anon());          //  InvokeResponseIB
  mtrc_tlv_start_struct(&w, mtrc_tlv_ctx(1));          //   status = CommandStatusIB
  mtrc_tlv_start_list(&w, mtrc_tlv_ctx(0));            //    CommandPath
  mtrc_tlv_put_uint(&w, mtrc_tlv_ctx(0), endpoint);
  mtrc_tlv_put_uint(&w, mtrc_tlv_ctx(1), cluster);
  mtrc_tlv_put_uint(&w, mtrc_tlv_ctx(2), command);
  mtrc_tlv_end_container(&w);                          //    end CommandPath
  mtrc_tlv_start_struct(&w, mtrc_tlv_ctx(1));          //    StatusIB
  mtrc_tlv_put_uint(&w, mtrc_tlv_ctx(0), status);      //     0: status
  mtrc_tlv_end_container(&w);                          //    end StatusIB
  mtrc_tlv_end_container(&w);                          //   end CommandStatusIB
  mtrc_tlv_end_container(&w);                          //  end InvokeResponseIB
  mtrc_tlv_end_container(&w);                          // end InvokeResponses
  mtrc_tlv_put_uint(&w, mtrc_tlv_ctx(0xFF), 1);        // interactionModelRevision
  mtrc_tlv_end_container(&w);                          // end message
  return mtrc_tlv_writer_ok(&w) ? (int)mtrc_tlv_writer_len(&w) : -1;
}
