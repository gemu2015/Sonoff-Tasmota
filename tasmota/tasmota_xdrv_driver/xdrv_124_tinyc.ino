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
 * Compiles TinyC source in browser IDE → uploads .tcb bytecode → runs on device
 *
 * Commands:
 *   TinyC          - Show VM status
 *   TinyCRun       - Run loaded program (1 = auto-run on boot)
 *   TinyCStop      - Stop running program
 *   TinyCReset     - Reset VM state
 *   TinyCExec <n>  - Set instructions per tick (default 1000)
 *
 * Web:
 *   /tc            - TinyC console page with upload form
 *   /tc_upload     - POST endpoint for .tcb binary upload
 *   /tc_api        - GET JSON API (cmd=run|stop|status) with CORS
\*********************************************************************************************/

#define XDRV_124  124

// VM engine is in a separate .h to avoid Arduino IDE auto-prototype issues
#include "xdrv_124_tinyc_vm.h"

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
  AddLog(LOG_LEVEL_INFO, PSTR("TCC: TinyC VM initialized (%d bytes, %d free)"), needed, ESP_getFreeHeap());

  // Try auto-load from filesystem
#ifdef USE_UFILESYS
  if (ffsp) {
    File file = ffsp->open(TC_FILE_NAME, "r");
    if (file) {
      uint32_t fsize = file.size();
      if (fsize > 0 && fsize <= TC_MAX_PROGRAM) {
        Tinyc->program = (uint8_t *)malloc(fsize);
        if (Tinyc->program) {
          file.read(Tinyc->program, fsize);
          Tinyc->program_size = fsize;
          int err = tc_vm_load(&Tinyc->vm, Tinyc->program, fsize);
          if (err == TC_OK) {
            Tinyc->loaded = true;
            AddLog(LOG_LEVEL_INFO, PSTR("TCC: Auto-loaded %s (%d bytes)"), TC_FILE_NAME, fsize);
          } else {
            AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Auto-load failed: %s"), tc_error_str(err));
            free(Tinyc->program);
            Tinyc->program = nullptr;
            Tinyc->program_size = 0;
          }
        }
      }
      file.close();
    }
  }
#endif
}

/*********************************************************************************************\
 * Tasmota: Periodic execution (every 50ms)
\*********************************************************************************************/

static void TinyCEvery50ms(void) {
  if (!Tinyc) return;

#ifdef ESP32
  // ESP32: VM runs in its own FreeRTOS task — just monitor for completion
  if (Tinyc->running && !Tinyc->task_running) {
    // Task finished — update state
    Tinyc->running = false;
    tc_output_flush();
  }
  return;
#else
  // ESP8266: slice-based execution in 50ms tick (no FreeRTOS task support)
  if (!Tinyc->loaded || !Tinyc->running) return;
  if (Tinyc->vm.halted || Tinyc->vm.error != TC_OK) {
    tc_free_all_frames(&Tinyc->vm);
    if (Tinyc->vm.halted && Tinyc->vm.error == TC_OK) {
      // Normal halt: globals + heap persist for callbacks
      tc_output_flush();
      AddLog(LOG_LEVEL_INFO, PSTR("TCC: Program halted after %u instructions, %d callbacks"),
        Tinyc->vm.instruction_count, Tinyc->vm.callback_count);
      Tinyc->running = false;
    }
    if (Tinyc->vm.error != TC_OK) {
      tc_heap_free_all(&Tinyc->vm);
      tc_output_flush();
      AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Error: %s (PC=%d)"),
        tc_error_str(Tinyc->vm.error), Tinyc->vm.pc - Tinyc->vm.code_offset);
      Tinyc->running = false;
    }
    return;
  }

  yield();  // Feed WDT before VM execution
  int err = tc_vm_run_slice(&Tinyc->vm, Tinyc->instr_per_tick);
  yield();  // Feed WDT after VM execution

  if (err != TC_OK && err != TC_ERR_PAUSED) {
    tc_free_all_frames(&Tinyc->vm);
    tc_heap_free_all(&Tinyc->vm);
    tc_output_flush();
    AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Runtime error: %s (PC=%d, instr=%u)"),
      tc_error_str(err), Tinyc->vm.pc - Tinyc->vm.code_offset, Tinyc->vm.instruction_count);
    Tinyc->running = false;
  }
#endif  // ESP32 vs ESP8266
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
  Response_P(PSTR("{\"TinyC\":{\"Loaded\":%d,\"Running\":%d,\"Size\":%d,"
    "\"PC\":%d,\"SP\":%d,\"Instr\":%u,\"Error\":\"%s\",\"Heap\":%d}}"),
    Tinyc->loaded ? 1 : 0,
    Tinyc->running ? 1 : 0,
    Tinyc->program_size,
    Tinyc->vm.pc - Tinyc->vm.code_offset,
    Tinyc->vm.sp,
    Tinyc->vm.instruction_count,
    tc_error_str(Tinyc->vm.error),
    ESP_getFreeHeap());
}

// Helper: start the VM (load + launch task on ESP32, or set running flag on ESP8266)
static bool TinyCStartVM(void) {
  if (!Tinyc || !Tinyc->loaded) return false;

  // Reset VM
  int err = tc_vm_load(&Tinyc->vm, Tinyc->program, Tinyc->program_size);
  if (err != TC_OK) return false;

  Tinyc->output_len = 0;
  Tinyc->output[0] = '\0';
  Tinyc->running = true;

#ifdef ESP32
  // Stop any existing task first
  if (Tinyc->task_handle) {
    Tinyc->task_stop = true;
    for (int i = 0; i < 50 && Tinyc->task_running; i++) { delay(10); }
    if (Tinyc->task_running) {
      vTaskDelete(Tinyc->task_handle);
      Tinyc->task_running = false;
    }
    Tinyc->task_handle = nullptr;
  }

  Tinyc->task_stop = false;
  Tinyc->task_running = false;

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2)
  // Single-core variants — no core affinity
  BaseType_t ret = xTaskCreate(tc_vm_task, "tinyc_vm", 4096, Tinyc, 1, &Tinyc->task_handle);
#else
  // Dual-core ESP32/S3 — pin to core 1
  BaseType_t ret = xTaskCreatePinnedToCore(tc_vm_task, "tinyc_vm", 4096, Tinyc, 1, &Tinyc->task_handle, 1);
#endif
  if (ret != pdPASS) {
    AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Failed to create task"));
    Tinyc->running = false;
    return false;
  }
#endif  // ESP32

  AddLog(LOG_LEVEL_INFO, PSTR("TCC: Program started"));
  return true;
}

// Helper: stop the VM (signal task to stop on ESP32, or clear flags on ESP8266)
static void TinyCStopVM(void) {
  if (!Tinyc) return;

#ifdef ESP32
  if (Tinyc->task_handle) {
    Tinyc->task_stop = true;
    // Wait for task to exit (max 500ms)
    for (int i = 0; i < 50 && Tinyc->task_running; i++) {
      delay(10);
    }
    if (Tinyc->task_running) {
      // Force kill if task didn't exit cleanly
      vTaskDelete(Tinyc->task_handle);
      Tinyc->task_running = false;
    }
    Tinyc->task_handle = nullptr;
  }
#endif

  Tinyc->running = false;
  Tinyc->vm.running = false;
  tc_free_all_frames(&Tinyc->vm);
  tc_heap_free_all(&Tinyc->vm);
  tc_udp_stop();
  tc_output_flush();
}

void CmndTinyCRun(void) {
  if (!Tinyc || !Tinyc->loaded) { ResponseCmndChar_P(PSTR("No program loaded")); return; }
  if (!TinyCStartVM()) {
    ResponseCmndChar_P(PSTR("Start failed"));
    return;
  }
  ResponseCmndDone();
}

void CmndTinyCStop(void) {
  if (!Tinyc) { ResponseCmndChar_P(TC_NOT_INIT); return; }
  TinyCStopVM();
  AddLog(LOG_LEVEL_INFO, PSTR("TCC: Program stopped"));
  ResponseCmndDone();
}

void CmndTinyCReset(void) {
  if (!Tinyc) { ResponseCmndChar_P(TC_NOT_INIT); return; }
  TinyCStopVM();  // also frees frame locals
  memset(&Tinyc->vm, 0, sizeof(TcVM));  // safe — pointers already freed
  Tinyc->output_len = 0;
  Tinyc->output[0] = '\0';
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

static void HandleTinyCPage(void) {
  if (!HttpCheckPriviledgedAccess()) { return; }

  WSContentStart_P(PSTR("TinyC Console"));
  WSContentSendStyle();

  // Handle button commands first (before displaying status)
  if (Tinyc && Webserver->hasArg(F("cmd"))) {
    String cmd = Webserver->arg(F("cmd"));
    if (cmd == "run") {
      TinyCStartVM();
    } else if (cmd == "stop") {
      TinyCStopVM();
    } else if (cmd == "reset") {
      CmndTinyCReset();
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

  // --- VM Status ---
  WSContentSend_P(PSTR("<fieldset><legend><b> TinyC VM </b></legend>"));
  if (Tinyc) {
    char state[10], state_class[10];
    if (Tinyc->running) { strcpy_P(state, PSTR("Running")); strcpy_P(state_class, PSTR("tc-run")); }
    else if (Tinyc->loaded) { strcpy_P(state, PSTR("Loaded")); strcpy_P(state_class, PSTR("tc-load")); }
    else { strcpy_P(state, PSTR("Empty")); strcpy_P(state_class, PSTR("tc-empty")); }

    char prog_state[8];
    strcpy_P(prog_state, Tinyc->loaded ? PSTR("Loaded") : PSTR("None"));
    WSContentSend_P(PSTR(
      "<div class='tc-stat'><table>"
      "<tr><td>Status</td><td><span class='%s'>&#x25cf; %s</span></td></tr>"
      "<tr><td>Program</td><td>%s (%d bytes)</td></tr>"
      "<tr><td>Instructions</td><td>%u</td></tr>"
      "<tr><td>PC / SP</td><td>%d / %d</td></tr>"),
      state_class, state,
      prog_state,
      Tinyc->program_size,
      Tinyc->vm.instruction_count,
      Tinyc->vm.pc - Tinyc->vm.code_offset, Tinyc->vm.sp);

    // Only show error row if there's an error
    if (Tinyc->vm.error != 0) {
      WSContentSend_P(PSTR("<tr><td>Error</td><td class='tc-err'>%s</td></tr>"),
        tc_error_str(Tinyc->vm.error));
    }

    WSContentSend_P(PSTR(
      "<tr><td>Instr/tick</td><td>%d</td></tr>"
      "</table></div>"),
      Tinyc->instr_per_tick);

    // Control buttons
    WSContentSend_P(PSTR(
      "<div class='tc-btns'><form action='/tc' method='get' style='display:flex;gap:8px;width:100%%'>"
      "<button name='cmd' value='run' class='button bgrn'>&#x25B6; Run</button>"
      "<button name='cmd' value='stop' class='button bred'>&#x25A0; Stop</button>"
      "<button name='cmd' value='reset' class='button'>&#x21BB; Reset</button>"
      "</form></div>"));

    // Output log
    if (Tinyc->output_len > 0) {
      WSContentSend_P(PSTR("<b>Output</b><div class='tc-out'>%s</div>"), Tinyc->output);
    }
  } else {
    WSContentSend_P(PSTR("<p style='text-align:center;opacity:.6'>TinyC not initialized</p>"));
  }
  WSContentSend_P(PSTR("</fieldset>"));

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

  // Check if this is an API call (from browser IDE) via ?api=1 query parameter
  bool is_api = Webserver->hasArg(F("api"));

  if (is_api) {
    // JSON response with CORS headers for browser IDE
    TCSendCORS("POST, OPTIONS");
    if (Tinyc && Tinyc->loaded) {
      char json[128];
      snprintf_P(json, sizeof(json), PSTR("{\"ok\":true,\"size\":%d}"), Tinyc->program_size);
      WSSendJSON(200, json);
    } else {
      WSSendJSON_P(400, PSTR("{\"ok\":false,\"error\":\"upload failed\"}"));
    }
    return;
  }

  // Regular HTML response for form-based upload from /tc page
  WSContentStart_P(PSTR("TinyC Upload"));
  WSContentSendStyle();

  if (Tinyc && Tinyc->loaded) {
    WSContentSend_P(PSTR(
      "<fieldset><legend><b> Upload Result </b></legend>"
      "<p style='text-align:center;color:#0a0'><b>&#x2714; Upload successful!</b></p>"
      "<p style='text-align:center'>Program size: %d bytes</p>"
      "</fieldset>"),
      Tinyc->program_size);
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
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: Upload start: %s"), upload.filename.c_str());
    // Stop any running program
    TinyCStopVM();
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
    if (Tinyc->upload_buf && Tinyc->upload_received > 0 && !Web.upload_error) {
      // Free old program
      if (Tinyc->program) { free(Tinyc->program); }

      // Use upload buffer as program
      Tinyc->program = Tinyc->upload_buf;
      Tinyc->program_size = Tinyc->upload_received;
      Tinyc->upload_buf = nullptr;

      // Try to load
      int err = tc_vm_load(&Tinyc->vm, Tinyc->program, Tinyc->program_size);
      if (err == TC_OK) {
        Tinyc->loaded = true;
        AddLog(LOG_LEVEL_INFO, PSTR("TCC: Loaded %d bytes"), Tinyc->program_size);

        // Save to filesystem
#ifdef USE_UFILESYS
        if (ffsp) {
          TfsSaveFile(TC_FILE_NAME, Tinyc->program, Tinyc->program_size);
          AddLog(LOG_LEVEL_INFO, PSTR("TCC: Saved to %s"), TC_FILE_NAME);
        }
#endif
      } else {
        AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Load error: %s"), tc_error_str(err));
        free(Tinyc->program);
        Tinyc->program = nullptr;
        Tinyc->program_size = 0;
        Tinyc->loaded = false;
        Web.upload_error = 1;
      }
    } else {
      if (Tinyc->upload_buf) { free(Tinyc->upload_buf); Tinyc->upload_buf = nullptr; }
    }
  }
}

// ─── API endpoint for browser IDE (JSON + CORS) ─────────────
// GET /tc_api?cmd=run|stop|status
static void HandleTinyCApi(void) {
  TCSendCORS("GET, OPTIONS");

  if (!Tinyc) {
    WSSendJSON_P(500, PSTR("{\"ok\":false,\"error\":\"not initialized\"}"));
    return;
  }

  String cmd = Webserver->arg(F("cmd"));
  char json[256];

  if (cmd == "run") {
    if (!Tinyc->loaded) {
      WSSendJSON_P(400, PSTR("{\"ok\":false,\"error\":\"no program loaded\"}"));
      return;
    }
    if (!TinyCStartVM()) {
      WSSendJSON_P(400, PSTR("{\"ok\":false,\"error\":\"start failed\"}"));
      return;
    }
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: Program started (API)"));
    snprintf_P(json, sizeof(json), PSTR("{\"ok\":true,\"running\":true,\"size\":%d}"), Tinyc->program_size);
    WSSendJSON(200, json);
  }
  else if (cmd == "stop") {
    TinyCStopVM();
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: Program stopped (API)"));
    WSSendJSON_P(200, PSTR("{\"ok\":true,\"running\":false}"));
  }
  else {
    // Default: status
    snprintf_P(json, sizeof(json),
      PSTR("{\"ok\":true,\"loaded\":%d,\"running\":%d,\"size\":%d,\"pc\":%d,\"sp\":%d,\"instr\":%u,\"error\":\"%s\",\"heap\":%d}"),
      Tinyc->loaded ? 1 : 0,
      Tinyc->running ? 1 : 0,
      Tinyc->program_size,
      Tinyc->vm.pc - Tinyc->vm.code_offset,
      Tinyc->vm.sp,
      Tinyc->vm.instruction_count,
      tc_error_str(Tinyc->vm.error),
      ESP_getFreeHeap());
    WSSendJSON(200, json);
  }
}

// CORS preflight for /tc_api
static void HandleTinyCApiCORS(void) {
  TCSendCORS("GET, OPTIONS");
  Webserver->send(204);
}

// ─── Self-hosted IDE (optional — #define USE_TINYC_IDE) ──────
// Serves /tinyc_ide.html (or .gz) from filesystem at /ide
#ifdef USE_TINYC_IDE
#ifdef USE_UFILESYS
static void HandleTinyCIde(void) {
  if (!ffsp) {
    WSSend_P(503, PSTR("text/plain"), PSTR("Filesystem not available"));
    return;
  }

  // Try gzipped version first
  bool gzipped = false;
  File f = ffsp->open("/tinyc_ide.html.gz", "r");
  if (f) {
    gzipped = true;
  } else {
    f = ffsp->open("/tinyc_ide.html", "r");
  }

  if (!f) {
    WSSend_P(404, PSTR("text/plain"), PSTR("TinyC IDE not found. Upload tinyc_ide.html to filesystem."));
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

#endif  // USE_WEBSERVER

/*********************************************************************************************\
 * Tasmota: JSON telemetry
\*********************************************************************************************/

static void TinyCShow(bool json) {
  if (!Tinyc) return;

  if (json) {
    // Always append basic TinyC status
    ResponseAppend_P(PSTR(",\"TinyC\":{\"Running\":%d,\"Loaded\":%d,\"Size\":%d,\"Instr\":%u}"),
      Tinyc->running ? 1 : 0,
      Tinyc->loaded ? 1 : 0,
      Tinyc->program_size,
      Tinyc->vm.instruction_count);
    // Call user's JsonCall() — uses responseAppend() to write directly to Tasmota JSON
    if (Tinyc->loaded && Tinyc->vm.halted && Tinyc->vm.error == TC_OK) {
      tc_vm_call_callback(&Tinyc->vm, "JsonCall");
    }
  }
#ifdef USE_WEBSERVER
  else {
    // Default web status row
    char status[10];
    if (Tinyc->running) { strcpy_P(status, PSTR("Running")); }
    else if (Tinyc->loaded) { strcpy_P(status, PSTR("Loaded")); }
    else { strcpy_P(status, PSTR("Empty")); }
    WSContentSend_PD(PSTR("{s}TinyC{m}%s (%d bytes){e}"),
      status,
      Tinyc->program_size);
    // Call user's WebCall() — uses webSend() to write directly to Tasmota web page
    if (Tinyc->loaded && Tinyc->vm.halted && Tinyc->vm.error == TC_OK) {
      tc_vm_call_callback(&Tinyc->vm, "WebCall");
    }
  }
#endif
}

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
      break;
    case FUNC_EVERY_50_MSECOND:
      TinyCEvery50ms();
      break;
    case FUNC_EVERY_SECOND:
      // Call user's EverySecond() callback if VM halted normally
      if (Tinyc->loaded && Tinyc->vm.halted && Tinyc->vm.error == TC_OK) {
        tc_vm_call_callback(&Tinyc->vm, "EverySecond");
      }
      break;
    case FUNC_COMMAND:
      result = DecodeCommand(kTinyCCommands, TinyCCommand);
      break;
    case FUNC_JSON_APPEND:
      TinyCShow(true);
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      TinyCShow(false);
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      // Call user's WebPage() — one-time page content (charts, custom HTML)
      if (Tinyc->loaded && Tinyc->vm.halted && Tinyc->vm.error == TC_OK) {
        tc_vm_call_callback(&Tinyc->vm, "WebPage");
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
