/*
  xdrv_124_tinyc.ino - TinyC Bytecode VM for Tasmota

  Copyright (C) 2024  Gerhard Mutz

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#ifdef USE_TINYC

/*********************************************************************************************\
 * TinyC - Lightweight C-subset bytecode VM for ESP32/ESP8266
 *
 * Multi-VM slot support: up to TC_MAX_VMS simultaneous programs (4 on ESP32, 1 on ESP8266)
 *
 * Commands:
 *   TinyC          - Show VM status (all slots)
 *   TinyCRun       - Run loaded program in slot 0 (or specify file)
 *   TinyCStop      - Stop running program in slot 0
 *   TinyCReset     - Reset VM state in slot 0
 *   TinyCExec <n>  - Set instructions per tick (default 1000)
 *
 * Web:
 *   /tc            - TinyC console page with upload form (shows all slots)
 *   /tc_upload     - POST endpoint for .tcb binary upload
 *   /tc_api        - GET JSON API (cmd=run|stop|status) with CORS, slot= parameter
\*********************************************************************************************/

#define XDRV_124  124

// Forward declarations for custom web handlers (called from SYS_WEB_ON in vm.h)
static void HandleTinyCWebOn1(void);
static void HandleTinyCWebOn2(void);
static void HandleTinyCWebOn3(void);
static void HandleTinyCWebOn4(void);
static void (*const TinyCWebOnHandlers[])(void) = {
  HandleTinyCWebOn1, HandleTinyCWebOn2, HandleTinyCWebOn3, HandleTinyCWebOn4
};

// mDNS support (for SYS_MDNS syscall)
#ifdef ESP32
  #include <ESPmDNS.h>
#else
  #include <ESP8266mDNS.h>
#endif

// UriGlob for port 82 download server wildcard routes
#include <uri/UriGlob.h>

// VM engine is in a separate .h to avoid Arduino IDE auto-prototype issues
#include "xdrv_124_tinyc_vm.h"

/*********************************************************************************************\
 * Helpers: slot-aware callback dispatch
\*********************************************************************************************/

// tc_slot_callback() is in vm.h (Arduino auto-prototype workaround)

// Call a named callback on ALL active (loaded + halted + no-error) slots
static void tc_all_callbacks(const char *name) {
  if (!Tinyc) return;
  for (uint8_t i = 0; i < TC_MAX_VMS; i++) {
    TcSlot *s = Tinyc->slots[i];
    if (s) tc_slot_callback(s, name);
  }
}

/*********************************************************************************************\
 * Tasmota: Init
\*********************************************************************************************/

static void TinyCInit(void) {
  uint32_t freeHeap = ESP_getFreeHeap();
  uint32_t needed = sizeof(struct TINYC);
  AddLog(LOG_LEVEL_INFO, PSTR("TCC: Need %d bytes, free heap %d"), needed, freeHeap);

  if (freeHeap < needed + 4096) {  // keep 4KB reserve
    AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Not enough heap (%d free, need %d+4K)"), freeHeap, needed);
    return;
  }

  Tinyc = (struct TINYC *)calloc(1, sizeof(struct TINYC));
  if (!Tinyc) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Memory allocation failed (%d bytes)"), needed);
    return;
  }
  Tinyc->instr_per_tick = TC_INSTR_PER_TICK;
  // Init SPI CS pins to -1 (unused)
  for (int i = 0; i < TC_SPI_MAX_CS; i++) { Tinyc->spi.cs[i] = -1; }
  Tinyc->spi.sclk = 0;  // 0 = not initialized (valid pins are >0 or <0)

  // Allocate slot 0 (default slot for backward compatibility)
  Tinyc->slots[0] = tc_slot_alloc();
  if (!Tinyc->slots[0]) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Slot 0 allocation failed"));
    free(Tinyc);
    Tinyc = nullptr;
    return;
  }

  AddLog(LOG_LEVEL_INFO, PSTR("TCC: TinyC VM initialized (%d bytes, %d free)"), needed, ESP_getFreeHeap());

  // Try auto-load from filesystem into slot 0
#ifdef USE_UFILESYS
  TinyCLoadFile(TC_FILE_NAME, 0);
#endif
}

// TinyCSetPersistFile(), TinyCStopVM(), TinyCStartVM() are in vm.h
// (Arduino auto-prototype workaround for TcSlot* parameters)

/*********************************************************************************************\
 * Tasmota: Periodic execution (every 50ms)
\*********************************************************************************************/

static void TinyCEvery50ms(void) {
  if (!Tinyc) return;

#ifdef ESP32
  // ESP32: VM runs in its own FreeRTOS task -- monitor all slots for completion
  for (uint8_t i = 0; i < TC_MAX_VMS; i++) {
    TcSlot *s = Tinyc->slots[i];
    if (!s) continue;
    if (s->running && !s->task_running) {
      // Task finished -- update state
      s->running = false;
      tc_current_slot = s;
      tc_output_flush();
      tc_current_slot = nullptr;
    }
  }
#else
  // ESP8266: slice-based execution in 50ms tick (no FreeRTOS task support)
  // Only slot 0 on ESP8266
  TcSlot *s = Tinyc->slots[0];
  if (s && s->loaded && s->running) {
    if (s->vm.halted || s->vm.error != TC_OK) {
      tc_free_all_frames(&s->vm);
      if (s->vm.halted && s->vm.error == TC_OK) {
        // Normal halt: globals + heap persist for callbacks
        tc_current_slot = s;
        tc_output_flush();
        tc_current_slot = nullptr;
        AddLog(LOG_LEVEL_INFO, PSTR("TCC: Program halted after %u instructions, %d callbacks"),
          s->vm.instruction_count, s->vm.callback_count);
        s->running = false;
      }
      if (s->vm.error != TC_OK) {
        tc_heap_free_all(&s->vm);
        tc_current_slot = s;
        tc_output_flush();
        tc_current_slot = nullptr;
        AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Error: %s (PC=%d)"),
          tc_error_str(s->vm.error), s->vm.pc - s->vm.code_offset);
        s->running = false;
      }
    } else {
      yield();  // Feed WDT before VM execution
      tc_current_slot = s;
      int err = tc_vm_run_slice(&s->vm, Tinyc->instr_per_tick);
      tc_current_slot = nullptr;
      yield();  // Feed WDT after VM execution

      if (err != TC_OK && err != TC_ERR_PAUSED) {
        tc_free_all_frames(&s->vm);
        tc_heap_free_all(&s->vm);
        tc_current_slot = s;
        tc_output_flush();
        tc_current_slot = nullptr;
        AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Runtime error: %s (PC=%d, instr=%u)"),
          tc_error_str(err), s->vm.pc - s->vm.code_offset, s->vm.instruction_count);
        s->running = false;
      }
    }
  }
#endif  // ESP32 vs ESP8266

  // Execute deferred commands (audio etc.) only when VM is halted and idle
  // Must not run while VM task is active -- concurrent SD access causes crashes
  // Check slot 0 for deferred exec (shared infrastructure)
  {
    TcSlot *s0 = Tinyc->slots[0];
    if (s0 && s0->loaded && s0->vm.halted && s0->vm.error == TC_OK) {
      tc_deferred_exec();
    }
  }

  // Every50ms callback on all active slots
  tc_all_callbacks("Every50ms");
}

/*********************************************************************************************\
 * Tasmota: Commands
\*********************************************************************************************/

static const char TC_NOT_INIT[] PROGMEM = "Not initialized";

#define D_PRFX_TINYC "TinyC"

const char kTinyCCommands[] PROGMEM = D_PRFX_TINYC "|"
  "|Run|Stop|Reset|Exec";

void (* const TinyCCommand[])(void) PROGMEM = {
  &CmndTinyC, &CmndTinyCRun, &CmndTinyCStop,
  &CmndTinyCReset, &CmndTinyCExec
};

void CmndTinyC(void) {
  if (!Tinyc) { ResponseCmndChar_P(TC_NOT_INIT); return; }
  // Show status for all slots
  Response_P(PSTR("{\"TinyC\":{\"Heap\":%d,\"Slots\":["), ESP_getFreeHeap());
  bool first = true;
  for (uint8_t i = 0; i < TC_MAX_VMS; i++) {
    TcSlot *s = Tinyc->slots[i];
    if (!s) continue;
    if (!first) ResponseAppend_P(PSTR(","));
    first = false;
    ResponseAppend_P(PSTR("{\"Slot\":%d,\"Loaded\":%d,\"Running\":%d,\"Size\":%d,"
      "\"PC\":%d,\"SP\":%d,\"Instr\":%u,\"Error\":\"%s\",\"File\":\"%s\"}"),
      i,
      s->loaded ? 1 : 0,
      s->running ? 1 : 0,
      s->program_size,
      s->vm.pc - s->vm.code_offset,
      s->vm.sp,
      s->vm.instruction_count,
      tc_error_str(s->vm.error),
      s->filename[0] ? s->filename : "");
  }
  ResponseAppend_P(PSTR("]}}"));
}

void CmndTinyCRun(void) {
  if (!Tinyc) { ResponseCmndChar_P(TC_NOT_INIT); return; }
  TcSlot *s = Tinyc->slots[0];
  if (!s) { ResponseCmndChar_P(TC_NOT_INIT); return; }
#ifdef USE_UFILESYS
  // If a filename is given (e.g., "TinyC Run /bresser.tcb"), load it first
  if (XdrvMailbox.data_len > 0 && XdrvMailbox.data[0] == '/') {
    TinyCStopVM(s);
    if (!TinyCLoadFile(XdrvMailbox.data, 0)) {
      ResponseCmndChar_P(PSTR("Load failed"));
      return;
    }
  }
#endif
  if (!s->loaded) { ResponseCmndChar_P(PSTR("No program loaded")); return; }
  if (!TinyCStartVM(s)) {
    ResponseCmndChar_P(PSTR("Start failed"));
    return;
  }
  ResponseCmndDone();
}

void CmndTinyCStop(void) {
  if (!Tinyc) { ResponseCmndChar_P(TC_NOT_INIT); return; }
  TcSlot *s = Tinyc->slots[0];
  if (!s) { ResponseCmndChar_P(TC_NOT_INIT); return; }
  TinyCStopVM(s);
  AddLog(LOG_LEVEL_INFO, PSTR("TCC: Program stopped"));
  ResponseCmndDone();
}

void CmndTinyCReset(void) {
  if (!Tinyc) { ResponseCmndChar_P(TC_NOT_INIT); return; }
  TcSlot *s = Tinyc->slots[0];
  if (!s) { ResponseCmndChar_P(TC_NOT_INIT); return; }
  TinyCStopVM(s);  // also frees frame locals
  memset(&s->vm, 0, sizeof(TcVM));  // safe -- pointers already freed
  s->output_len = 0;
  s->output[0] = '\0';
  AddLog(LOG_LEVEL_INFO, PSTR("TCC: VM reset"));
  ResponseCmndDone();
}

void CmndTinyCExec(void) {
  if (!Tinyc) { ResponseCmndChar_P(TC_NOT_INIT); return; }
  if (XdrvMailbox.payload > 0) {
    Tinyc->instr_per_tick = XdrvMailbox.payload;
  }
  ResponseCmndNumber(Tinyc->instr_per_tick);
}

/*********************************************************************************************\
 * Tasmota: Web interface
\*********************************************************************************************/

#ifdef USE_WEBSERVER

// Helper: send response with PROGMEM body string (saves RAM on ESP8266)
static void WSSend_P(int code, PGM_P content_type, PGM_P body) {
  char ct[32], buf[96];
  strncpy_P(ct, content_type, sizeof(ct) - 1); ct[sizeof(ct) - 1] = '\0';
  strncpy_P(buf, body, sizeof(buf) - 1); buf[sizeof(buf) - 1] = '\0';
  Webserver->send(code, (const char*)ct, (const char*)buf);
}
// Shared PROGMEM strings for web responses
static const char TC_CT_JSON[] PROGMEM = "application/json";
static const char TC_CORS_ORIGIN[] PROGMEM = "Access-Control-Allow-Origin";
static const char TC_CORS_METHODS[] PROGMEM = "Access-Control-Allow-Methods";
static const char TC_CORS_HEADERS[] PROGMEM = "Access-Control-Allow-Headers";

// Send CORS headers from PROGMEM (saves ~180 bytes RAM on ESP8266)
static void TCSendCORS(const char *methods_ram) {
  char hdr[40], val[16];
  strncpy_P(hdr, TC_CORS_ORIGIN, sizeof(hdr)); Webserver->sendHeader(hdr, "*");
  strncpy_P(hdr, TC_CORS_METHODS, sizeof(hdr)); Webserver->sendHeader(hdr, methods_ram);
  strncpy_P(hdr, TC_CORS_HEADERS, sizeof(hdr));
  strcpy_P(val, PSTR("Content-Type"));
  Webserver->sendHeader(hdr, val);
}
// Shorthand for JSON responses
#define WSSendJSON_P(code, body) WSSend_P(code, TC_CT_JSON, body)
// Send JSON with pre-filled RAM buffer
static void WSSendJSON(int code, const char *json_buf) {
  char ct[20];
  strncpy_P(ct, TC_CT_JSON, sizeof(ct) - 1); ct[sizeof(ct) - 1] = '\0';
  Webserver->send(code, (const char*)ct, json_buf);
}

// Helper: load a .tcb file from filesystem into a specified slot
#ifdef USE_UFILESYS
static bool TinyCLoadFile(const char *path, uint8_t slot_num) {
  if (!Tinyc) return false;
  if (slot_num >= TC_MAX_VMS) return false;

  // Allocate slot if needed
  if (!Tinyc->slots[slot_num]) {
    Tinyc->slots[slot_num] = tc_slot_alloc();
    if (!Tinyc->slots[slot_num]) {
      AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Slot %d alloc failed"), slot_num);
      return false;
    }
  }
  TcSlot *s = Tinyc->slots[slot_num];

  File file;
  if (ufsp) file = ufsp->open(path, "r");
  if (!file && ffsp && ffsp != ufsp) file = ffsp->open(path, "r");
  if (!file) return false;
  uint32_t fsize = file.size();
  if (fsize == 0 || fsize > TC_MAX_PROGRAM) { file.close(); return false; }
  TinyCStopVM(s);
  if (s->program) { free(s->program); s->program = nullptr; }
  s->program = (uint8_t *)malloc(fsize);
  if (!s->program) { file.close(); return false; }
  file.read(s->program, fsize);
  file.close();
  s->program_size = fsize;
  int err = tc_vm_load(&s->vm, s->program, fsize);
  if (err == TC_OK) {
    s->loaded = true;
    strlcpy(s->filename, path, sizeof(s->filename));
    TinyCSetPersistFile(s, path);
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: Loaded %s (%d bytes) into slot %d"), path, fsize, slot_num);
    return true;
  }
  AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Load %s failed: %s"), path, tc_error_str(err));
  free(s->program);
  s->program = nullptr;
  s->program_size = 0;
  return false;
}
#endif

static void HandleTinyCPage(void) {
  if (!HttpCheckPriviledgedAccess()) { return; }

  WSContentStart_P(PSTR("TinyC Console"));
  WSContentSendStyle();

  // Handle button commands first (before displaying status)
  // Commands default to slot 0 unless otherwise specified
  if (Tinyc && Webserver->hasArg(F("cmd"))) {
    String cmd = Webserver->arg(F("cmd"));
    uint8_t cmd_slot = 0;
    if (Webserver->hasArg(F("slot"))) {
      cmd_slot = Webserver->arg(F("slot")).toInt();
      if (cmd_slot >= TC_MAX_VMS) cmd_slot = 0;
    }
    TcSlot *cs = Tinyc->slots[cmd_slot];

    if (cmd == "run" && cs) {
      TinyCStartVM(cs);
    } else if (cmd == "stop" && cs) {
      TinyCStopVM(cs);
    } else if (cmd == "reset" && cs) {
      TinyCStopVM(cs);
      memset(&cs->vm, 0, sizeof(TcVM));
      cs->output_len = 0;
      cs->output[0] = '\0';
      AddLog(LOG_LEVEL_INFO, PSTR("TCC: VM slot %d reset (web)"), cmd_slot);
    } else if (cmd == "load" && Webserver->hasArg(F("file"))) {
#ifdef USE_UFILESYS
      String file = Webserver->arg(F("file"));
      if (file.length() > 0) {
        TinyCLoadFile(file.c_str(), cmd_slot);
      }
#endif
    } else if (cmd == "delall") {
#ifdef USE_UFILESYS
      // Delete all .tcb files from both filesystems
      int total = 0;
      FS *fss[] = { ufsp, (ffsp && ffsp != ufsp) ? ffsp : nullptr };
      for (int fi = 0; fi < 2; fi++) {
        if (!fss[fi]) continue;
        File dir = fss[fi]->open("/", "r");
        if (!dir) continue;
        char names[16][40];
        int count = 0;
        dir.rewindDirectory();
        while (count < 16) {
          File entry = dir.openNextFile();
          if (!entry) break;
          if (!entry.isDirectory()) {
            char *ep = (char *)entry.name();
            if (*ep == '/') ep++;
            char *lcp = strrchr(ep, '/');
            if (lcp) ep = lcp + 1;
            uint16_t nlen = strlen(ep);
            if (nlen > 4 && strcasecmp(ep + nlen - 4, ".tcb") == 0) {
              snprintf(names[count], sizeof(names[0]), "/%s", ep);
              count++;
            }
          }
          entry.close();
        }
        dir.close();
        for (int i = 0; i < count; i++) {
          fss[fi]->remove(names[i]);
        }
        total += count;
      }
      AddLog(LOG_LEVEL_INFO, PSTR("TCC: Deleted %d .tcb files"), total);
#endif
    }
  }

  // Custom styles for this page
  WSContentSend_P(PSTR(
    "<style>"
    ".tc-stat{background:var(--c_frm);border-radius:.3em;padding:10px 14px;margin:8px 0}"
    ".tc-stat table{width:100%%}"
    ".tc-stat td{padding:4px 8px}"
    ".tc-stat td:first-child{color:var(--c_txt);opacity:.7;width:120px}"
    ".tc-stat td:last-child{font-weight:bold}"
    ".tc-run{color:#0a0}.tc-load{color:#fa0}.tc-empty{color:var(--c_txt);opacity:.5}"
    ".tc-err{color:#f44}"
    ".tc-btns{display:flex;gap:8px;margin:10px 0}"
    ".tc-btns button{width:auto;flex:1;padding:0 12px}"
    ".tc-out{background:#1a1a1a;color:#0f0;padding:10px;border-radius:.3em;"
    "max-height:200px;overflow:auto;font-family:monospace;font-size:.9em;"
    "white-space:pre-wrap;word-break:break-all;margin:8px 0}"
    ".tc-sect{margin-top:16px;padding-top:12px;border-top:1px solid var(--c_btn)}"
    ".tc-upload input[type=file]{margin:8px 0}"
    ".tc-ide-url{display:flex;gap:8px;align-items:center}"
    ".tc-ide-url input{flex:1;padding:6px 8px}"
    ".tc-ide-url button{width:auto;padding:0 16px}"
    "</style>"));

  // --- VM Status for ALL slots ---
  if (Tinyc) {
    for (uint8_t si = 0; si < TC_MAX_VMS; si++) {
      TcSlot *s = Tinyc->slots[si];
      if (!s) continue;

      char legend[40];
      snprintf_P(legend, sizeof(legend), PSTR(" TinyC VM [Slot %d] "), si);
      WSContentSend_P(PSTR("<fieldset><legend><b>%s</b></legend>"), legend);

      char state[10], state_class[10];
      if (s->running) { strcpy_P(state, PSTR("Running")); strcpy_P(state_class, PSTR("tc-run")); }
      else if (s->loaded) { strcpy_P(state, PSTR("Loaded")); strcpy_P(state_class, PSTR("tc-load")); }
      else { strcpy_P(state, PSTR("Empty")); strcpy_P(state_class, PSTR("tc-empty")); }

      WSContentSend_P(PSTR(
        "<div class='tc-stat'><table>"
        "<tr><td>Status</td><td><span class='%s'>&#x25cf; %s</span></td></tr>"
        "<tr><td>Program</td><td>%s (%d bytes)</td></tr>"
        "<tr><td>Instructions</td><td>%u</td></tr>"
        "<tr><td>PC / SP</td><td>%d / %d</td></tr>"),
        state_class, state,
        s->filename[0] ? s->filename : (s->loaded ? "loaded" : "none"),
        s->program_size,
        s->vm.instruction_count,
        s->vm.pc - s->vm.code_offset, s->vm.sp);

      // Only show error row if there's an error
      if (s->vm.error != 0) {
        WSContentSend_P(PSTR("<tr><td>Error</td><td class='tc-err'>%s</td></tr>"),
          tc_error_str(s->vm.error));
      }

      WSContentSend_P(PSTR(
        "<tr><td>Instr/tick</td><td>%d</td></tr>"
        "</table></div>"),
        Tinyc->instr_per_tick);

      // Control buttons (per-slot)
      WSContentSend_P(PSTR(
        "<div class='tc-btns'><form action='/tc' method='get' style='display:flex;gap:8px;width:100%%'>"
        "<input type='hidden' name='slot' value='%d'>"
        "<button name='cmd' value='run' class='button bgrn'>&#x25B6; Run</button>"
        "<button name='cmd' value='stop' class='button bred'>&#x25A0; Stop</button>"
        "<button name='cmd' value='reset' class='button'>&#x21BB; Reset</button>"
        "</form></div>"), si);

      // Output log for this slot
      if (s->output_len > 0) {
        WSContentSend_P(PSTR("<b>Output</b><div class='tc-out'>%s</div>"), s->output);
      }

      WSContentSend_P(PSTR("</fieldset>"));
    }

    // --- File selector (shared, loads into slot 0 by default) ---
#ifdef USE_UFILESYS
    if (ufsp || ffsp) {
      TcSlot *s0 = Tinyc->slots[0];
      WSContentSend_P(PSTR(
        "<fieldset><legend><b> Load Program </b></legend>"
        "<p><form action='/tc' method='get'>"
        "<select name='file' style='width:100%%'>"));
      // Scan up to 2 filesystems: ufsp (SD/main) and ffsp (flash) if different
      FS *fss[] = { ufsp, (ffsp && ffsp != ufsp) ? ffsp : nullptr };
      const char *fslabel[] = { "", " [flash]" };
      for (int fi = 0; fi < 2; fi++) {
        if (!fss[fi]) continue;
        File dir = fss[fi]->open("/", "r");
        if (!dir) continue;
        dir.rewindDirectory();
        while (true) {
          File entry = dir.openNextFile();
          if (!entry) break;
          if (entry.isDirectory()) { entry.close(); continue; }
          char *ep = (char *)entry.name();
          if (*ep == '/') ep++;
          char *lcp = strrchr(ep, '/');
          if (lcp) ep = lcp + 1;
          uint16_t nlen = strlen(ep);
          if (nlen > 4 && strcasecmp(ep + nlen - 4, ".tcb") == 0) {
            char fpath[40];
            snprintf(fpath, sizeof(fpath), "/%s", ep);
            bool is_current = (s0 && s0->filename[0] && strcmp(fpath, s0->filename) == 0);
            WSContentSend_P(PSTR("<option value='%s'%s>%s (%d B)%s</option>"),
              fpath, is_current ? " selected" : "", ep, entry.size(), fslabel[fi]);
          }
          entry.close();
        }
        dir.close();
      }
      WSContentSend_P(PSTR(
        "</select>"
        "<br><div style='display:flex;gap:8px'>"
        "<button name='cmd' value='load' class='button'>Load</button>"
        "<button name='cmd' value='delall' class='button bred'"
        " onclick=\"return confirm('Delete all .tcb files?')\">"
        "Delete All .tcb</button>"
        "</div></form></p></fieldset>"));
    }
#endif

  } else {
    WSContentSend_P(PSTR("<fieldset><legend><b> TinyC VM </b></legend>"
      "<p style='text-align:center;opacity:.6'>TinyC not initialized</p>"
      "</fieldset>"));
  }

  // --- Upload Section ---
  WSContentSend_P(PSTR(
    "<fieldset><legend><b> Upload Program </b></legend>"
    "<form class='tc-upload' method='POST' action='/tc_upload' enctype='multipart/form-data'>"
    "<input type='file' name='tcb' accept='.tcb'>"
    "<button type='submit' class='button bgrn'>Upload .tcb</button>"
    "</form></fieldset>"));

  // --- IDE Section ---
  WSContentSend_P(PSTR("<fieldset><legend><b> TinyC IDE </b></legend>"));
#if defined(USE_TINYC_IDE) && defined(USE_UFILESYS)
  WSContentSend_P(PSTR(
    "<p style='text-align:center'>"
    "<button onclick=\"window.open('/ide')\" class='button bgrn'>Open IDE</button>"
    "</p>"
    "<p style='text-align:center;font-size:.85em;opacity:.6'>Served from device filesystem</p>"));
#else
  WSContentSend_P(PSTR(
    "<div class='tc-ide-url'>"
    "<input id='ide_url' value='http://localhost:8080' placeholder='IDE URL'>"
    "<button onclick=\"var u=document.getElementById('ide_url').value;"
    "window.open(u+'?device='+location.hostname)\" class='button bgrn'>Open</button>"
    "</div>"
    "<p style='text-align:center;font-size:.85em;opacity:.6'>IDE URL saved in browser</p>"
    "<script>var u=localStorage.getItem('tinyc_ide_url');"
    "if(u)document.getElementById('ide_url').value=u;"
    "document.getElementById('ide_url').onchange=function(){"
    "localStorage.setItem('tinyc_ide_url',this.value)};</script>"));
#endif
  WSContentSend_P(PSTR("</fieldset>"));

  WSContentSpaceButton(BUTTON_MAIN);
  WSContentEnd();
}

static void HandleTinyCUploadDone(void) {
  if (!HttpCheckPriviledgedAccess()) { return; }

  uint8_t slot_num = Tinyc ? Tinyc->upload_slot : 0;
  TcSlot *s = (Tinyc && slot_num < TC_MAX_VMS) ? Tinyc->slots[slot_num] : nullptr;

  // Check if this is an API call (from browser IDE) via ?api=1 query parameter
  bool is_api = Webserver->hasArg(F("api"));

  if (is_api) {
    // JSON response with CORS headers for browser IDE
    TCSendCORS("POST, OPTIONS");
    if (s && s->loaded) {
      char json[160];
      snprintf_P(json, sizeof(json), PSTR("{\"ok\":true,\"size\":%d,\"file\":\"%s\",\"slot\":%d}"),
        s->program_size,
        s->filename[0] ? s->filename : "",
        slot_num);
      WSSendJSON(200, json);
    } else {
      WSSendJSON_P(400, PSTR("{\"ok\":false,\"error\":\"upload failed\"}"));
    }
    return;
  }

  // Regular HTML response for form-based upload from /tc page
  WSContentStart_P(PSTR("TinyC Upload"));
  WSContentSendStyle();

  if (s && s->loaded) {
    WSContentSend_P(PSTR(
      "<fieldset><legend><b> Upload Result </b></legend>"
      "<p style='text-align:center;color:#0a0'><b>&#x2714; Upload successful!</b></p>"
      "<p style='text-align:center'>%s — %d bytes (slot %d)</p>"
      "</fieldset>"),
      s->filename[0] ? s->filename : "program",
      s->program_size, slot_num);
  } else {
    WSContentSend_P(PSTR(
      "<fieldset><legend><b> Upload Result </b></legend>"
      "<p style='text-align:center;color:#f44'><b>&#x2718; Upload failed</b></p>"
      "</fieldset>"));
  }
  WSContentSend_P(HTTP_FORM_BUTTON, PSTR("tc"), PSTR("Back to TinyC"));

  WSContentSpaceButton(BUTTON_MAIN);
  WSContentEnd();
}

// Handle CORS preflight for browser IDE uploads
static void HandleTinyCUploadCORS(void) {
  TCSendCORS("POST, OPTIONS");
  Webserver->send(204);
}

static void HandleTinyCUpload(void) {
  if (!Tinyc) return;

  HTTPUpload& upload = Webserver->upload();

  if (upload.status == UPLOAD_FILE_START) {
    // Determine target slot from ?slot=N parameter (default 0)
    Tinyc->upload_slot = 0;
    if (Webserver->hasArg(F("slot"))) {
      uint8_t rs = Webserver->arg(F("slot")).toInt();
      if (rs < TC_MAX_VMS) Tinyc->upload_slot = rs;
    }
    uint8_t slot_num = Tinyc->upload_slot;

    AddLog(LOG_LEVEL_INFO, PSTR("TCC: Upload start: %s (slot %d)"), upload.filename.c_str(), slot_num);

    // Capture uploaded filename (prepend / for filesystem path)
    snprintf(Tinyc->upload_filename, sizeof(Tinyc->upload_filename), "/%s", upload.filename.c_str());

    // Allocate slot if needed
    if (!Tinyc->slots[slot_num]) {
      Tinyc->slots[slot_num] = tc_slot_alloc();
      if (!Tinyc->slots[slot_num]) {
        Web.upload_error = 1;
        AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Slot %d alloc failed"), slot_num);
        return;
      }
    }
    TcSlot *s = Tinyc->slots[slot_num];

    // Stop any running program in this slot
    TinyCStopVM(s);
    // Allocate upload buffer
    if (Tinyc->upload_buf) { free(Tinyc->upload_buf); Tinyc->upload_buf = nullptr; }
    Tinyc->upload_buf = (uint8_t *)malloc(TC_MAX_PROGRAM);
    if (!Tinyc->upload_buf) {
      Web.upload_error = 1;
      AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Upload malloc failed"));
      return;
    }
    Tinyc->upload_received = 0;
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Tinyc->upload_buf && Tinyc->upload_received + upload.currentSize <= TC_MAX_PROGRAM) {
      memcpy(Tinyc->upload_buf + Tinyc->upload_received, upload.buf, upload.currentSize);
      Tinyc->upload_received += upload.currentSize;
    } else {
      Web.upload_error = 1;
      AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Upload too large (max %d)"), TC_MAX_PROGRAM);
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    uint8_t slot_num = Tinyc->upload_slot;
    TcSlot *s = (slot_num < TC_MAX_VMS) ? Tinyc->slots[slot_num] : nullptr;

    if (s && Tinyc->upload_buf && Tinyc->upload_received > 0 && !Web.upload_error) {
      // Free old program in this slot
      if (s->program) { free(s->program); }

      // Use upload buffer as program
      s->program = Tinyc->upload_buf;
      s->program_size = Tinyc->upload_received;
      Tinyc->upload_buf = nullptr;

      // Try to load
      int err = tc_vm_load(&s->vm, s->program, s->program_size);
      if (err == TC_OK) {
        s->loaded = true;
        strlcpy(s->filename, Tinyc->upload_filename, sizeof(s->filename));
        TinyCSetPersistFile(s, s->filename);
        AddLog(LOG_LEVEL_INFO, PSTR("TCC: Loaded %d bytes into slot %d"), s->program_size, slot_num);

        // Save to filesystem with uploaded filename
#ifdef USE_UFILESYS
        if (ufsp) {
          const char *saveName = s->filename[0] ? s->filename : TC_FILE_NAME;
          TfsSaveFile(saveName, s->program, s->program_size);
          AddLog(LOG_LEVEL_INFO, PSTR("TCC: Saved to %s"), saveName);
        }
#endif
      } else {
        AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Load error: %s"), tc_error_str(err));
        free(s->program);
        s->program = nullptr;
        s->program_size = 0;
        s->loaded = false;
        Web.upload_error = 1;
      }
    } else {
      if (Tinyc->upload_buf) { free(Tinyc->upload_buf); Tinyc->upload_buf = nullptr; }
    }
  }
}

// ---- API endpoint for browser IDE (JSON + CORS) ----
// GET /tc_api?cmd=run|stop|status&slot=N
static void HandleTinyCApi(void) {
  TCSendCORS("GET, OPTIONS");

  if (!Tinyc) {
    WSSendJSON_P(500, PSTR("{\"ok\":false,\"error\":\"not initialized\"}"));
    return;
  }

  String cmd = Webserver->arg(F("cmd"));
  uint8_t slot_num = 0;
  if (Webserver->hasArg(F("slot"))) {
    slot_num = Webserver->arg(F("slot")).toInt();
    if (slot_num >= TC_MAX_VMS) slot_num = 0;
  }
  char json[384];

  if (cmd == "run") {
    // Allocate slot if needed
    if (!Tinyc->slots[slot_num]) {
      Tinyc->slots[slot_num] = tc_slot_alloc();
    }
    TcSlot *s = Tinyc->slots[slot_num];
    if (!s || !s->loaded) {
      WSSendJSON_P(400, PSTR("{\"ok\":false,\"error\":\"no program loaded\"}"));
      return;
    }
    if (!TinyCStartVM(s)) {
      WSSendJSON_P(400, PSTR("{\"ok\":false,\"error\":\"start failed\"}"));
      return;
    }
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: Program started (API, slot %d)"), slot_num);
    snprintf_P(json, sizeof(json), PSTR("{\"ok\":true,\"running\":true,\"size\":%d,\"slot\":%d}"),
      s->program_size, slot_num);
    WSSendJSON(200, json);
  }
  else if (cmd == "stop") {
    TcSlot *s = Tinyc->slots[slot_num];
    if (s) {
      TinyCStopVM(s);
      AddLog(LOG_LEVEL_INFO, PSTR("TCC: Program stopped (API, slot %d)"), slot_num);
    }
    WSSendJSON_P(200, PSTR("{\"ok\":true,\"running\":false}"));
  }
  else if (cmd == "status") {
    // Return array of all slot statuses
    String result = F("{\"ok\":true,\"slots\":[");
    bool first = true;
    for (uint8_t i = 0; i < TC_MAX_VMS; i++) {
      TcSlot *s = Tinyc->slots[i];
      if (!s) continue;
      if (!first) result += ',';
      first = false;
      snprintf_P(json, sizeof(json),
        PSTR("{\"slot\":%d,\"loaded\":%d,\"running\":%d,\"size\":%d,\"file\":\"%s\","
             "\"pc\":%d,\"sp\":%d,\"instr\":%u,\"error\":\"%s\"}"),
        i,
        s->loaded ? 1 : 0,
        s->running ? 1 : 0,
        s->program_size,
        s->filename[0] ? s->filename : "",
        s->vm.pc - s->vm.code_offset,
        s->vm.sp,
        s->vm.instruction_count,
        tc_error_str(s->vm.error));
      result += json;
    }
    result += F("],\"heap\":");
    result += String(ESP_getFreeHeap());
    result += '}';
    TCSendCORS("GET, OPTIONS");
    Webserver->send(200, F("application/json"), result);
    return;
  }
  else if (cmd == "freegpio") {
    // Return list of free (usable, not flash, not assigned) GPIO pins
    TCSendCORS("GET, OPTIONS");
    String result = F("{\"ok\":true,\"gpios\":[");
    bool first = true;
    for (uint32_t i = 0; i < MAX_GPIO_PIN; i++) {
      if (FlashPin(i)) continue;                    // skip flash/reserved pins
      if (TasmotaGlobal.gpio_pin[i] > 0) continue;  // skip assigned pins
      if (!first) result += ',';
      result += String(i);
      first = false;
    }
    result += F("]}");
    Webserver->send(200, F("application/json"), result);
    return;
  }
#ifdef USE_UFILESYS
  else if (cmd == "listfiles") {
    // List files on device filesystem (both ufsp and ffsp)
    if (!ufsp && !ffsp) {
      WSSendJSON_P(500, PSTR("{\"ok\":false,\"error\":\"no filesystem\"}"));
      return;
    }
    String result = F("{\"ok\":true,\"files\":[");
    bool first = true;
    FS *fss[] = { ufsp, (ffsp && ffsp != ufsp) ? ffsp : nullptr };
    for (int fi = 0; fi < 2; fi++) {
      if (!fss[fi]) continue;
      File root = fss[fi]->open("/", "r");
      if (!root) continue;
      File f = root.openNextFile();
      while (f) {
        if (!f.isDirectory()) {
          const char *fname = f.name();
          if (*fname == '/') fname++;
          const char *lcp = strrchr(fname, '/');
          if (lcp) fname = lcp + 1;
          if (!first) result += ',';
          result += F("{\"name\":\"");
          result += fname;
          result += F("\",\"size\":");
          result += String(f.size());
          if (fi == 1) result += F(",\"fs\":\"flash\"");
          result += '}';
          first = false;
        }
        f = root.openNextFile();
      }
      root.close();
    }
    result += F("]}");
    TCSendCORS("GET, OPTIONS");
    Webserver->send(200, F("application/json"), result);
    return;
  }
  else if (cmd == "deletefile") {
    // Delete a file: /tc_api?cmd=deletefile&path=/filename (tries ufsp then ffsp)
    String fpath = Webserver->arg(F("path"));
    if (fpath.length() == 0 || fpath[0] != '/') {
      WSSendJSON_P(400, PSTR("{\"ok\":false,\"error\":\"missing path\"}"));
      return;
    }
    if (!ufsp && !ffsp) {
      WSSendJSON_P(500, PSTR("{\"ok\":false,\"error\":\"no filesystem\"}"));
      return;
    }
    bool deleted = false;
    if (ufsp) deleted = ufsp->remove(fpath.c_str());
    if (!deleted && ffsp && ffsp != ufsp) deleted = ffsp->remove(fpath.c_str());
    if (deleted) {
      WSSendJSON_P(200, PSTR("{\"ok\":true}"));
    } else {
      WSSendJSON_P(404, PSTR("{\"ok\":false,\"error\":\"delete failed\"}"));
    }
    return;
  }
  else if (cmd == "readfile") {
    // Read a text file from filesystem: /tc_api?cmd=readfile&path=/sml_meter.def
    // Supports time-range filter: /tc_api?cmd=readfile&path=/data.csv@1.2.22-00:00_12.2.22-00:00
    String fpath = Webserver->arg(F("path"));
    if (fpath.length() == 0 || fpath[0] != '/') {
      WSSendJSON_P(400, PSTR("{\"ok\":false,\"error\":\"missing path\"}"));
      return;
    }
    if (!ufsp) {
      WSSendJSON_P(500, PSTR("{\"ok\":false,\"error\":\"no filesystem\"}"));
      return;
    }

    // Check for time-range filter: path@from_to
    char pathbuf[128];
    strlcpy(pathbuf, fpath.c_str(), sizeof(pathbuf));
    uint32_t cmp_from = 0, cmp_to = 0;
    char *atp = strchr(pathbuf, '@');
    if (atp) {
      *atp = 0;
      atp++;
      // from_to separated by underscore
      char *tp = strchr(atp, '_');
      if (tp) {
        *tp = 0;
        tp++;
        cmp_from = tc_ts_cmp(atp);
        cmp_to = tc_ts_cmp(tp);
      }
    }

    File f = ufsp->open(pathbuf, "r");
    if (!f) {
      WSSendJSON_P(404, PSTR("{\"ok\":false,\"error\":\"file not found\"}"));
      return;
    }

    TCSendCORS("GET, POST, OPTIONS");

    if (cmp_from && cmp_to && cmp_to > cmp_from) {
      // -- Time-filtered file serving --
      Webserver->setContentLength(CONTENT_LENGTH_UNKNOWN);
      Webserver->send(200, F("text/plain"), "");

      char *lbuf = (char*)malloc(512);
      if (!lbuf) { f.close(); return; }

      // 1. Send header line (first line of CSV)
      uint16_t li = 0;
      while (f.available() && li < 510) {
        uint8_t c;
        f.read(&c, 1);
        lbuf[li++] = c;
        if (c == '\n') break;
      }
      lbuf[li] = 0;
      if (li > 0) Webserver->sendContent(lbuf);
      uint32_t header_end = f.position();

      // 2. Try index file for fast seek
      uint32_t seek_pos = 0;
      {
        char indpath[128];
        strlcpy(indpath, pathbuf, sizeof(indpath));
        char *dot = strrchr(indpath, '.');
        if (dot) {
          strcpy(dot, ".ind");
        } else {
          strcat(indpath, ".ind");
        }
        File ind = ufsp->open(indpath, "r");
        if (ind) {
          // Skip index header line
          while (ind.available()) {
            uint8_t c; ind.read(&c, 1);
            if (c == '\n') break;
          }
          // Scan index lines: timestamp\tbyte_offset
          uint32_t last_good_pos = 0;
          uint16_t ycnt = 0;
          while (ind.available()) {
            li = 0;
            while (ind.available() && li < 510) {
              uint8_t c; ind.read(&c, 1);
              lbuf[li++] = c;
              if (c == '\n') break;
            }
            lbuf[li] = 0;
            uint32_t cmp = tc_ts_cmp(lbuf);
            if (cmp == 0) continue;
            if (cmp >= cmp_from) break;  // past our start
            char *tab = strchr(lbuf, '\t');
            if (tab) {
              last_good_pos = strtoul(tab + 1, NULL, 10);
            }
            if (++ycnt >= 100) { ycnt = 0; yield(); }
          }
          seek_pos = last_good_pos;
          ind.close();
        } else {
          // No index: estimated seek (like Scripter's opt_fext)
          li = 0;
          while (f.available() && li < 31) {
            uint8_t c; f.read(&c, 1);
            if (c == '\t' || c == '\n') break;
            lbuf[li++] = c;
          }
          lbuf[li] = 0;
          uint32_t ts_first = tc_ts_cmp(lbuf);

          // Find last line timestamp
          uint32_t fsize = f.size();
          uint32_t back = (fsize > 256) ? fsize - 256 : 0;
          f.seek(back, SeekSet);
          uint32_t last_nl = 0;
          while (f.available()) {
            uint8_t c; f.read(&c, 1);
            if (c == '\n') last_nl = f.position();
          }
          if (last_nl > back) {
            f.seek(last_nl, SeekSet);
            li = 0;
            while (f.available() && li < 31) {
              uint8_t c; f.read(&c, 1);
              if (c == '\t' || c == '\n') break;
              lbuf[li++] = c;
            }
            lbuf[li] = 0;
            uint32_t ts_last = tc_ts_cmp(lbuf);
            if (ts_last > ts_first && cmp_from > ts_first) {
              float perc = (float)(cmp_from - ts_first) / (float)(ts_last - ts_first) * 0.8f;
              if (perc < 0) perc = 0;
              if (perc > 1) perc = 1;
              seek_pos = (uint32_t)(perc * fsize);
            }
          }
          // Skip partial line at seek position
          if (seek_pos > 0) {
            f.seek(seek_pos, SeekSet);
            while (f.available()) {
              uint8_t c; f.read(&c, 1);
              if (c == '\n') break;
            }
            seek_pos = f.position();
          }
        }
      }

      // 3. Seek to start position and stream matching lines
      if (seek_pos > 0) {
        f.seek(seek_pos, SeekSet);
      } else {
        f.seek(header_end, SeekSet);
      }

      while (f.available()) {
        li = 0;
        while (f.available() && li < 510) {
          uint8_t c; f.read(&c, 1);
          lbuf[li++] = c;
          if (c == '\n') break;
        }
        lbuf[li] = 0;

        // Extract timestamp from first column (before first tab)
        char saved = 0;
        char *tab = strchr(lbuf, '\t');
        if (tab) { saved = *tab; *tab = 0; }
        uint32_t cmp = tc_ts_cmp(lbuf);
        if (tab) *tab = saved;

        if (cmp == 0) continue;         // skip invalid/header
        if (cmp > cmp_to) break;        // past end, done (data is chronological)
        if (cmp >= cmp_from) {
          Webserver->sendContent(lbuf);
        }
        yield();  // feed WDT during streaming
      }
      // Signal end of chunked response
      Webserver->sendContent("");

      free(lbuf);
      f.close();
      return;
    }

    // Normal full-file streaming (no time filter)
    Webserver->setContentLength(f.size());
    Webserver->send(200, F("text/plain"), "");
    uint8_t buf[256];
    while (f.available()) {
      int n = f.read(buf, sizeof(buf));
      if (n > 0) Webserver->client().write(buf, n);
    }
    f.close();
    return;
  }
  else if (cmd == "writefile") {
    // Write text to a file: /tc_api?cmd=writefile&path=/sml_meter.def  (POST body = content)
    String fpath = Webserver->arg(F("path"));
    if (fpath.length() == 0 || fpath[0] != '/') {
      WSSendJSON_P(400, PSTR("{\"ok\":false,\"error\":\"missing path\"}"));
      return;
    }
    if (!ufsp) {
      WSSendJSON_P(500, PSTR("{\"ok\":false,\"error\":\"no filesystem\"}"));
      return;
    }
    String body = Webserver->arg(F("plain"));
    File f = ufsp->open(fpath.c_str(), "w");
    if (!f) {
      WSSendJSON_P(500, PSTR("{\"ok\":false,\"error\":\"write failed\"}"));
      return;
    }
    f.print(body);
    f.close();
    snprintf_P(json, sizeof(json), PSTR("{\"ok\":true,\"size\":%d}"), body.length());
    WSSendJSON(200, json);
    return;
  }
#endif
  else {
    // Default: status for a specific slot (backward-compatible single-slot response)
    TcSlot *s = Tinyc->slots[slot_num];
    if (s) {
      snprintf_P(json, sizeof(json),
        PSTR("{\"ok\":true,\"slot\":%d,\"loaded\":%d,\"running\":%d,\"size\":%d,\"file\":\"%s\","
             "\"pc\":%d,\"sp\":%d,\"instr\":%u,\"error\":\"%s\",\"heap\":%d}"),
        slot_num,
        s->loaded ? 1 : 0,
        s->running ? 1 : 0,
        s->program_size,
        s->filename[0] ? s->filename : "",
        s->vm.pc - s->vm.code_offset,
        s->vm.sp,
        s->vm.instruction_count,
        tc_error_str(s->vm.error),
        ESP_getFreeHeap());
    } else {
      snprintf_P(json, sizeof(json),
        PSTR("{\"ok\":true,\"slot\":%d,\"loaded\":0,\"running\":0,\"size\":0,\"heap\":%d}"),
        slot_num, ESP_getFreeHeap());
    }
    WSSendJSON(200, json);
  }
}

// CORS preflight for /tc_api
static void HandleTinyCApiCORS(void) {
  TCSendCORS("GET, OPTIONS");
  Webserver->send(204);
}

// ---- Self-hosted IDE (optional -- #define USE_TINYC_IDE) ----
// Serves /tinyc_ide.html (or .gz) from filesystem at /ide
#ifdef USE_TINYC_IDE
#ifdef USE_UFILESYS
static void HandleTinyCIde(void) {
  if (!ffsp && !ufsp) {
    WSSend_P(503, PSTR("text/plain"), PSTR("Filesystem not available"));
    return;
  }

  // Try gzipped version first on ffsp (flash), then ufsp (SD)
  bool gzipped = false;
  File f;
  if (ffsp) f = ffsp->open("/tinyc_ide.html.gz", "r");
  if (f) {
    gzipped = true;
  } else {
    if (ffsp) f = ffsp->open("/tinyc_ide.html", "r");
    if (!f && ufsp && ufsp != ffsp) {
      f = ufsp->open("/tinyc_ide.html.gz", "r");
      if (f) {
        gzipped = true;
      } else {
        f = ufsp->open("/tinyc_ide.html", "r");
      }
    }
  }

  if (!f) {
    WSSend_P(404, PSTR("text/plain"), PSTR("TinyC IDE not found. Upload tinyc_ide.html.gz to filesystem."));
    return;
  }

  uint32_t fsize = f.size();
  if (gzipped) {
    Webserver->sendHeader(F("Content-Encoding"), F("gzip"));
  }
  Webserver->setContentLength(fsize);
  WSSend_P(200, PSTR("text/html"), PSTR(""));

  // Stream file in chunks (smaller buffer on ESP8266 to save stack)
  uint8_t buf[256];
  while (f.available()) {
    int n = f.read(buf, sizeof(buf));
    if (n > 0) {
      Webserver->client().write(buf, n);
    }
    yield();
  }
  f.close();
}

#endif  // USE_UFILESYS
#endif  // USE_TINYC_IDE

// ---- WebUI: shared sv= parameter handler ----

// Process sv= widget value updates from AJAX requests
// Format: sv=gidx_value | sv=gidx_s_string | sv=gidx_t_HH:MM
// Uses slot 0 for globals access (WebUI is bound to slot 0)
static void TinyC_WebSetVar(void) {
  if (!Tinyc) return;
  TcSlot *s = Tinyc->slots[0];
  if (!s || !s->loaded) return;
  if (!Webserver->hasArg(F("sv"))) return;

  String sv = Webserver->arg(F("sv"));
  int sep = sv.indexOf('_');
  if (sep > 0) {
    int32_t gidx = sv.substring(0, sep).toInt();
    String val = sv.substring(sep + 1);
    if (gidx >= 0 && gidx < TC_MAX_GLOBALS) {
      if (val.startsWith("s_")) {
        // String value: write chars as int32 into globals[gidx..]
        const char *str = val.c_str() + 2;
        int32_t maxLen = TC_MAX_GLOBALS - gidx - 1;
        int i;
        for (i = 0; i < maxLen && str[i]; i++) {
          s->vm.globals[gidx + i] = (int32_t)(uint8_t)str[i];
        }
        s->vm.globals[gidx + i] = 0;  // null terminate
      } else if (val.startsWith("t_")) {
        // Time value: HH:MM -> HHMM integer
        const char *ts = val.c_str() + 2;
        int hh = 0, mm = 0;
        sscanf(ts, "%d:%d", &hh, &mm);
        s->vm.globals[gidx] = hh * 100 + mm;
      } else {
        s->vm.globals[gidx] = val.toInt();
      }
    }
  }
}

// ---- Custom web handlers (webOn) -- uses slot 0 ----

static void HandleTinyCWebOn(uint8_t handler_num) {
  if (!Tinyc) { Webserver->send(503, "text/plain", "TinyC not ready"); return; }
  TcSlot *s = Tinyc->slots[0];
  if (!s || !s->loaded || !s->vm.halted || s->vm.error != TC_OK) {
    Webserver->send(503, "text/plain", "TinyC not ready");
    return;
  }
  Tinyc->current_web_handler = handler_num;
  // CORS + chunked response -- callback uses webSend() to emit content
  TCSendCORS("GET, POST, OPTIONS");
  WSContentBegin(200, CT_PLAIN);
  tc_slot_callback(s, "WebOn");
  WSContentEnd();
  Tinyc->current_web_handler = 0;
}

static void HandleTinyCWebOn1(void) { HandleTinyCWebOn(1); }
static void HandleTinyCWebOn2(void) { HandleTinyCWebOn(2); }
static void HandleTinyCWebOn3(void) { HandleTinyCWebOn(3); }
static void HandleTinyCWebOn4(void) { HandleTinyCWebOn(4); }

// ---- WebUI: interactive widget page (/tc_ui) -- uses slot 0 ----

static void HandleTinyCUI(void) {
  if (!HttpCheckPriviledgedAccess()) return;
  if (!Tinyc) { Webserver->send(503, "text/plain", "TinyC not ready"); return; }
  TcSlot *s = Tinyc->slots[0];
  if (!s || !s->loaded || !s->vm.halted || s->vm.error != TC_OK) {
    Webserver->send(503, "text/plain", "TinyC not ready");
    return;
  }

  // Read page number from ?p= parameter (0-5, default 0)
  uint8_t page = 0;
  if (Webserver->hasArg(F("p"))) {
    page = Webserver->arg(F("p")).toInt();
    if (page >= TC_MAX_WEB_PAGES) page = 0;
  }
  Tinyc->current_page = page;

  // Handle sv= parameter -- widget value update
  TinyC_WebSetVar();

  // AJAX mode (m=1): just re-render widgets via WebUI() callback
  if (Webserver->hasArg(F("m"))) {
    WSContentBegin(200, CT_HTML);
    tc_slot_callback(s, "WebUI");
    WSContentEnd();
    return;
  }

  // Full page: HTML skeleton with JavaScript for AJAX refresh
  const char *title = (page < Tinyc->page_count && Tinyc->page_label[page][0])
                    ? Tinyc->page_label[page] : "TinyC UI";
  WSContentStart_P(title);
  WSContentSendStyle();
  WSContentSend_P(PSTR(
    "<script>"
    "var rfsh=1,x=null,lt;"
    "function la(p){"
      "var a=p||'';"
      "if(p)clearTimeout(lt);"
      "if(x)x.abort();"
      "x=new XMLHttpRequest();"
      "x.onreadystatechange=function(){"
        "if(x.readyState==4&&x.status==200){"
          "document.getElementById('ui').innerHTML=x.responseText;"
        "}"
      "};"
      "if(rfsh){"
        "x.open('GET','./tc_ui?p=%d&m=1'+a,true);"
        "x.send();"
        "lt=setTimeout(la,2000);"
      "}"
    "}"
    "function seva(v,i){la('&sv='+i+'_'+v);}"
    "function siva(v,i){rfsh=1;la('&sv='+i+'_s_'+v);rfsh=0;}"
    "function sivat(v,i){rfsh=1;la('&sv='+i+'_t_'+v);rfsh=0;}"
    "function pr(f){if(f){lt=setTimeout(la,2000);rfsh=1;}else{clearTimeout(lt);rfsh=0;}}"
    "window.onload=la;"
    "</script>"
  ), page);
  WSContentSend_P(PSTR("<div id='ui'>"));
  tc_slot_callback(s, "WebUI");
  WSContentSend_P(PSTR("</div>"));
  WSContentSpaceButton(BUTTON_MAIN);
  WSContentEnd();
}

#endif  // USE_WEBSERVER

/*********************************************************************************************\
 * Tasmota: JSON telemetry
\*********************************************************************************************/

static void TinyCShow(bool json) {
  if (!Tinyc) return;

  if (json) {
    // Iterate all slots for JSON output
    for (uint8_t i = 0; i < TC_MAX_VMS; i++) {
      TcSlot *s = Tinyc->slots[i];
      if (!s) continue;
      if (i == 0) {
        // Slot 0 uses backward-compatible key "TinyC"
        ResponseAppend_P(PSTR(",\"TinyC\":{\"Running\":%d,\"Loaded\":%d,\"Size\":%d,\"Instr\":%u}"),
          s->running ? 1 : 0,
          s->loaded ? 1 : 0,
          s->program_size,
          s->vm.instruction_count);
      } else {
        ResponseAppend_P(PSTR(",\"TinyC%d\":{\"Running\":%d,\"Loaded\":%d,\"Size\":%d,\"Instr\":%u}"),
          i,
          s->running ? 1 : 0,
          s->loaded ? 1 : 0,
          s->program_size,
          s->vm.instruction_count);
      }
      // Call user's JsonCall() on this slot
      if (s->loaded && s->vm.halted && s->vm.error == TC_OK) {
        tc_slot_callback(s, "JsonCall");
      }
    }
  }
#ifdef USE_WEBSERVER
  else {
    // Web sensor rows for all slots
    for (uint8_t i = 0; i < TC_MAX_VMS; i++) {
      TcSlot *s = Tinyc->slots[i];
      if (!s) continue;
      char status[10];
      if (s->running) { strcpy_P(status, PSTR("Running")); }
      else if (s->loaded) { strcpy_P(status, PSTR("Loaded")); }
      else { strcpy_P(status, PSTR("Empty")); }
      if (i == 0) {
        WSContentSend_PD(PSTR("{s}TinyC{m}%s (%d bytes){e}"),
          status, s->program_size);
      } else {
        WSContentSend_PD(PSTR("{s}TinyC[%d]{m}%s (%d bytes){e}"),
          i, status, s->program_size);
      }
      // Call user's WebCall() on this slot
      if (s->loaded && s->vm.halted && s->vm.error == TC_OK) {
        tc_slot_callback(s, "WebCall");
      }
    }
  }
#endif
}

/*********************************************************************************************\
 * Port 82 Download Server -- background task for large file serving (ESP32 only)
 * Serves /ufs/<filename> with optional @from_to time-range filtering
 * Downloads run in a FreeRTOS task so main loop stays responsive
\*********************************************************************************************/

#ifdef ESP32

// Background task: streams file to client then exits
static void tc_download_task(void *param) {
  char *path = (char*)param;

  // Parse @from_to time range from path
  uint32_t cmp_from = 0, cmp_to = 0;
  char *atp = strchr(path, '@');
  if (atp) {
    *atp = 0;
    atp++;
    char *tp = strchr(atp, '_');
    if (tp) {
      *tp = 0;
      tp++;
      cmp_from = tc_ts_cmp(atp);
      cmp_to = tc_ts_cmp(tp);
    }
  }

  File file = ufsp->open(path, "r");
  if (!file) {
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: DL file not found: %s"), path);
    free(path);
    Tinyc->dl_busy = false;
    vTaskDelete(NULL);
    return;
  }

  // Determine content type
  char ctype[32];
  if (strstr_P(path, PSTR(".csv")) || strstr_P(path, PSTR(".txt"))) {
    strcpy_P(ctype, PSTR("text/plain"));
  } else if (strstr_P(path, PSTR(".html"))) {
    strcpy_P(ctype, PSTR("text/html"));
  } else if (strstr_P(path, PSTR(".json"))) {
    strcpy_P(ctype, PSTR("application/json"));
  } else {
    strcpy_P(ctype, PSTR("application/octet-stream"));
  }

  WiFiClient client = Tinyc->dl_server->client();
  uint32_t fsize = file.size();

  // Extract just the filename for Content-Disposition
  char *fname = strrchr(path, '/');
  if (fname) fname++; else fname = path;

  if (cmp_from && cmp_to && cmp_to > cmp_from) {
    // -- Time-filtered download --
    client.printf_P(PSTR("HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
      "Content-Disposition: attachment; filename=\"%s\"\r\n"
      "Transfer-Encoding: chunked\r\n"
      "Connection: close\r\n\r\n"), ctype, fname);

    char *lbuf = (char*)malloc(512);
    if (lbuf) {
      // Send header line
      uint16_t li = 0;
      while (file.available() && li < 510) {
        uint8_t c; file.read(&c, 1);
        lbuf[li++] = c;
        if (c == '\n') break;
      }
      if (li > 0) {
        // Chunked encoding: send size + data + CRLF
        client.printf("%x\r\n", li);
        client.write((uint8_t*)lbuf, li);
        client.print("\r\n");
      }
      uint32_t header_end = file.position();

      // Try index file for fast seek
      uint32_t seek_pos = 0;
      {
        char indpath[128];
        strlcpy(indpath, path, sizeof(indpath));
        char *dot = strrchr(indpath, '.');
        if (dot) strcpy(dot, ".ind");
        else strcat(indpath, ".ind");

        File ind = ufsp->open(indpath, "r");
        if (ind) {
          // Skip header
          while (ind.available()) {
            uint8_t c; ind.read(&c, 1);
            if (c == '\n') break;
          }
          uint32_t last_good_pos = 0;
          while (ind.available()) {
            li = 0;
            while (ind.available() && li < 510) {
              uint8_t c; ind.read(&c, 1);
              lbuf[li++] = c;
              if (c == '\n') break;
            }
            lbuf[li] = 0;
            uint32_t cmp = tc_ts_cmp(lbuf);
            if (cmp == 0) continue;
            if (cmp >= cmp_from) break;
            char *tab = strchr(lbuf, '\t');
            if (tab) last_good_pos = strtoul(tab + 1, NULL, 10);
          }
          seek_pos = last_good_pos;
          ind.close();
        } else {
          // No index: estimated seek (opt_fext approach)
          li = 0;
          while (file.available() && li < 31) {
            uint8_t c; file.read(&c, 1);
            if (c == '\t' || c == '\n') break;
            lbuf[li++] = c;
          }
          lbuf[li] = 0;
          uint32_t ts_first = tc_ts_cmp(lbuf);

          uint32_t back = (fsize > 256) ? fsize - 256 : 0;
          file.seek(back, SeekSet);
          uint32_t last_nl = 0;
          while (file.available()) {
            uint8_t c; file.read(&c, 1);
            if (c == '\n') last_nl = file.position();
          }
          if (last_nl > back) {
            file.seek(last_nl, SeekSet);
            li = 0;
            while (file.available() && li < 31) {
              uint8_t c; file.read(&c, 1);
              if (c == '\t' || c == '\n') break;
              lbuf[li++] = c;
            }
            lbuf[li] = 0;
            uint32_t ts_last = tc_ts_cmp(lbuf);
            if (ts_last > ts_first && cmp_from > ts_first) {
              float perc = (float)(cmp_from - ts_first) / (float)(ts_last - ts_first) * 0.8f;
              if (perc < 0) perc = 0;
              if (perc > 1) perc = 1;
              seek_pos = (uint32_t)(perc * fsize);
            }
          }
          if (seek_pos > 0) {
            file.seek(seek_pos, SeekSet);
            while (file.available()) {
              uint8_t c; file.read(&c, 1);
              if (c == '\n') break;
            }
            seek_pos = file.position();
          }
        }
      }

      // Stream matching lines
      if (seek_pos > 0) file.seek(seek_pos, SeekSet);
      else file.seek(header_end, SeekSet);

      while (file.available()) {
        li = 0;
        while (file.available() && li < 510) {
          uint8_t c; file.read(&c, 1);
          lbuf[li++] = c;
          if (c == '\n') break;
        }
        lbuf[li] = 0;

        char saved = 0;
        char *tab = strchr(lbuf, '\t');
        if (tab) { saved = *tab; *tab = 0; }
        uint32_t cmp = tc_ts_cmp(lbuf);
        if (tab) *tab = saved;

        if (cmp == 0) continue;
        if (cmp > cmp_to) break;
        if (cmp >= cmp_from) {
          client.printf("%x\r\n", li);
          client.write((uint8_t*)lbuf, li);
          client.print("\r\n");
        }
        yield();  // feed WDT during long transfers
      }
      // Chunked encoding terminator
      client.print("0\r\n\r\n");
      free(lbuf);
    }
  } else {
    // -- Full file download --
    client.printf_P(PSTR("HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
      "Content-Disposition: attachment; filename=\"%s\"\r\n"
      "Content-Length: %d\r\n"
      "Connection: close\r\n\r\n"), ctype, fname, fsize);

    uint8_t buf[512];
    while (fsize > 0) {
      uint16_t len = (fsize < sizeof(buf)) ? fsize : sizeof(buf);
      file.read(buf, len);
      client.write(buf, len);
      fsize -= len;
      yield();  // feed WDT during long transfers
    }
  }

  file.close();
  client.stop();
  free(path);
  Tinyc->dl_busy = false;
  AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: DL task done"));
  vTaskDelete(NULL);
}

// HTTP handler for /ufs/* requests on port 82
static void TC_DLServeFile(void) {
  String uri = Tinyc->dl_server->uri();
  const char *cp = strstr_P(uri.c_str(), PSTR("/ufs/"));
  if (!cp) {
    Tinyc->dl_server->send(404, F("text/plain"), F("Not found"));
    return;
  }
  cp += 4;  // skip "/ufs" -> keep leading "/"

  if (!ufsp) {
    Tinyc->dl_server->send(500, F("text/plain"), F("No filesystem"));
    return;
  }

  if (Tinyc->dl_busy) {
    Tinyc->dl_server->send(503, F("text/plain"), F("Download busy"));
    return;
  }

  Tinyc->dl_busy = true;
  char *path = (char*)malloc(128);
  if (!path) {
    Tinyc->dl_busy = false;
    Tinyc->dl_server->send(500, F("text/plain"), F("Out of memory"));
    return;
  }
  strlcpy(path, cp, 128);
  xTaskCreatePinnedToCore(tc_download_task, "TCDL", 6000, (void*)path, 3, NULL, 1);
}

// Root handler
static void TC_DLRoot(void) {
  Tinyc->dl_server->send(200, F("text/plain"), F("TinyC File Server"));
}

// Initialize port 82 download server
static void TC_DLServerInit(void) {
  if (!Tinyc || Tinyc->dl_server) return;  // already initialized
  Tinyc->dl_server = new ESP8266WebServer(TC_DLPORT);
  if (Tinyc->dl_server) {
    Tinyc->dl_server->on(UriGlob("/ufs/*"), HTTP_GET, TC_DLServeFile);
    Tinyc->dl_server->on("/", HTTP_GET, TC_DLRoot);
    Tinyc->dl_server->begin();
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: Download server started on port %d"), TC_DLPORT);
  }
}

// Poll for incoming connections (called from FUNC_LOOP)
static void TC_DLServerLoop(void) {
  if (Tinyc && Tinyc->dl_server) {
    Tinyc->dl_server->handleClient();
  }
}

#endif // ESP32

/*********************************************************************************************\
 * Tasmota: Driver entry point
\*********************************************************************************************/

bool Xdrv124(uint32_t function) {
  bool result = false;

  if (FUNC_INIT == function) {
    TinyCInit();
    return false;
  }

  if (!Tinyc) { return false; }

  switch (function) {
    case FUNC_LOOP:
      // Poll UDP multicast for incoming variables
      tc_udp_poll();
#ifdef ESP32
      // Lazy-init port 82 download server once WiFi is connected
      if (!Tinyc->dl_server && WifiHasIP()) {
        TC_DLServerInit();
      }
      // Poll port 82 download server for incoming file requests
      TC_DLServerLoop();
#endif
      // Call user's EveryLoop() callback on all active slots
      tc_all_callbacks("EveryLoop");
      break;
    case FUNC_EVERY_50_MSECOND:
      TinyCEvery50ms();
      break;
    case FUNC_EVERY_SECOND:
      // Call user's EverySecond() callback on all active slots
      tc_all_callbacks("EverySecond");
      break;
    case FUNC_COMMAND:
      result = DecodeCommand(kTinyCCommands, TinyCCommand);
      break;
    case FUNC_JSON_APPEND:
      TinyCShow(true);
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_GET_ARG:
      // Process sv= widget value updates from main page AJAX (slot 0)
      TinyC_WebSetVar();
      break;
    case FUNC_WEB_SENSOR:
      TinyCShow(false);
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      // Call user's WebPage() on all active slots
      for (uint8_t i = 0; i < TC_MAX_VMS; i++) {
        TcSlot *s = Tinyc->slots[i];
        if (!s || !s->loaded || !s->vm.halted || s->vm.error != TC_OK) continue;
        tc_slot_callback(s, "WebPage");
      }
      // Inject JavaScript for widget interactions on main page (slot 0 only)
      {
        TcSlot *s0 = Tinyc->slots[0];
        if (s0 && s0->loaded && s0->vm.halted && s0->vm.error == TC_OK) {
          if (tc_has_callback(&s0->vm, "WebCall")) {
            WSContentSend_P(PSTR(
              "<script>"
              "function seva(v,i){la('&sv='+i+'_'+v);}"
              "function siva(v,i){la('&sv='+i+'_s_'+v);}"
              "function sivat(v,i){la('&sv='+i+'_t_'+v);}"
              "function pr(f){if(f){lt=setTimeout(la,%d);}else{clearTimeout(lt);clearTimeout(ft);}}"
              "</script>"
            ), Settings->web_refresh);
          }
          // Add buttons to /tc_ui pages if WebUI callback is defined
          if (tc_has_callback(&s0->vm, "WebUI")) {
            if (Tinyc->page_count > 0) {
              // Multiple pages registered via wLabel()
              for (uint8_t p = 0; p < Tinyc->page_count; p++) {
                if (Tinyc->page_label[p][0]) {
                  WSContentSend_P(PSTR("<p></p><form action='tc_ui' method='get'>"
                    "<input type='hidden' name='p' value='%d'>"
                    "<button>%s</button></form>"), p, Tinyc->page_label[p]);
                }
              }
            } else {
              // No wLabel() called -- single default button
              WSContentSend_P(PSTR("<p></p><form action='tc_ui' method='get'><button>TinyC UI</button></form>"));
            }
          }
        }
      }
      break;
    case FUNC_WEB_ADD_CONSOLE_BUTTON:
      if (XdrvMailbox.index) {
        XdrvMailbox.index++;
      } else {
        WSContentSend_P(HTTP_FORM_BUTTON, PSTR("tc"), PSTR("TinyC Console"));
      }
      break;
    case FUNC_WEB_ADD_HANDLER:
      WebServer_on(PSTR("/tc"), HandleTinyCPage);
      Webserver->on("/tc_upload", HTTP_POST, HandleTinyCUploadDone, HandleTinyCUpload);
      Webserver->on("/tc_upload", HTTP_OPTIONS, HandleTinyCUploadCORS);
      WebServer_on(PSTR("/tc_api"), HandleTinyCApi);
      Webserver->on("/tc_api", HTTP_OPTIONS, HandleTinyCApiCORS);
      WebServer_on(PSTR("/tc_ui"), HandleTinyCUI);
#if defined(USE_TINYC_IDE) && defined(USE_UFILESYS)
      WebServer_on(PSTR("/ide"), HandleTinyCIde);
#endif
      break;
#endif
    case FUNC_ACTIVE:
      result = true;
      break;
  }
  return result;
}

#endif  // USE_TINYC
