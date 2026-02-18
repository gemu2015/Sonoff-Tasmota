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
    if (Tinyc->vm.halted) {
      tc_output_flush();
      AddLog(LOG_LEVEL_INFO, PSTR("TCC: Program halted after %u instructions"), Tinyc->vm.instruction_count);
      Tinyc->running = false;
    }
    if (Tinyc->vm.error != TC_OK) {
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

#define D_PRFX_TINYC "TinyC"

const char kTinyCCommands[] PROGMEM = D_PRFX_TINYC "|"
  "|Run|Stop|Reset|Exec";

void (* const TinyCCommand[])(void) PROGMEM = {
  &CmndTinyC, &CmndTinyCRun, &CmndTinyCStop,
  &CmndTinyCReset, &CmndTinyCExec
};

void CmndTinyC(void) {
  if (!Tinyc) { ResponseCmndChar("Not initialized"); return; }
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
  tc_output_flush();
}

void CmndTinyCRun(void) {
  if (!Tinyc || !Tinyc->loaded) { ResponseCmndChar("No program loaded"); return; }
  if (!TinyCStartVM()) {
    ResponseCmndChar("Start failed");
    return;
  }
  ResponseCmndDone();
}

void CmndTinyCStop(void) {
  if (!Tinyc) { ResponseCmndChar("Not initialized"); return; }
  TinyCStopVM();
  AddLog(LOG_LEVEL_INFO, PSTR("TCC: Program stopped"));
  ResponseCmndDone();
}

void CmndTinyCReset(void) {
  if (!Tinyc) { ResponseCmndChar("Not initialized"); return; }
  TinyCStopVM();
  memset(&Tinyc->vm, 0, sizeof(TcVM));
  Tinyc->output_len = 0;
  Tinyc->output[0] = '\0';
  AddLog(LOG_LEVEL_INFO, PSTR("TCC: VM reset"));
  ResponseCmndDone();
}

void CmndTinyCExec(void) {
  if (!Tinyc) { ResponseCmndChar("Not initialized"); return; }
  if (XdrvMailbox.payload > 0) {
    Tinyc->instr_per_tick = XdrvMailbox.payload;
  }
  ResponseCmndNumber(Tinyc->instr_per_tick);
}

/*********************************************************************************************\
 * Tasmota: Web interface
\*********************************************************************************************/

#ifdef USE_WEBSERVER

static void HandleTinyCPage(void) {
  if (!HttpCheckPriviledgedAccess()) { return; }

  WSContentStart_P(PSTR("TinyC Console"));
  WSContentSendStyle();

  // Status
  WSContentSend_P(PSTR("<h2>TinyC VM</h2>"));
  if (Tinyc) {
    WSContentSend_P(PSTR(
      "<table style='width:100%%'>"
      "<tr><td>Status</td><td><b>%s</b></td></tr>"
      "<tr><td>Program</td><td>%s (%d bytes)</td></tr>"
      "<tr><td>Instructions</td><td>%u</td></tr>"
      "<tr><td>PC / SP</td><td>%d / %d</td></tr>"
      "<tr><td>Error</td><td>%s</td></tr>"
      "<tr><td>Instr/tick</td><td>%d</td></tr>"
      "</table>"),
      Tinyc->running ? "Running" : (Tinyc->loaded ? "Loaded" : "Empty"),
      Tinyc->loaded ? "Loaded" : "None",
      Tinyc->program_size,
      Tinyc->vm.instruction_count,
      Tinyc->vm.pc - Tinyc->vm.code_offset, Tinyc->vm.sp,
      tc_error_str(Tinyc->vm.error),
      Tinyc->instr_per_tick);

    // Control buttons
    WSContentSend_P(PSTR(
      "<br><form action='/tc' method='get'>"
      "<button name='cmd' value='run'>Run</button> "
      "<button name='cmd' value='stop'>Stop</button> "
      "<button name='cmd' value='reset'>Reset</button>"
      "</form>"));

    // Handle button commands
    if (Webserver->hasArg(F("cmd"))) {
      String cmd = Webserver->arg(F("cmd"));
      if (cmd == "run") {
        if (TinyCStartVM()) {
          WSContentSend_P(PSTR("<p style='color:green'>Program started</p>"));
        } else {
          WSContentSend_P(PSTR("<p style='color:red'>Start failed</p>"));
        }
      } else if (cmd == "stop") {
        TinyCStopVM();
        WSContentSend_P(PSTR("<p style='color:orange'>Program stopped</p>"));
      } else if (cmd == "reset") {
        CmndTinyCReset();
        WSContentSend_P(PSTR("<p>VM reset</p>"));
      }
    }

    // Output log
    if (Tinyc->output_len > 0) {
      WSContentSend_P(PSTR("<h3>Output</h3><pre style='background:#222;color:#0f0;padding:8px;max-height:200px;overflow:auto'>%s</pre>"),
        Tinyc->output);
    }
  } else {
    WSContentSend_P(PSTR("<p>TinyC not initialized</p>"));
  }

  // Upload form
  WSContentSend_P(PSTR(
    "<hr><h3>Upload Program (.tcb)</h3>"
    "<form method='POST' action='/tc_upload' enctype='multipart/form-data'>"
    "<input type='file' name='tcb' accept='.tcb'>"
    "<br><br><button type='submit'>Upload</button>"
    "</form>"));

  // Open IDE button
#if defined(USE_TINYC_IDE) && defined(USE_UFILESYS)
  WSContentSend_P(PSTR(
    "<hr><h3>TinyC IDE</h3>"
    "<p><button onclick=\"window.open('/ide')\" style='padding:6px 16px'>Open IDE (on device)</button></p>"
    "<p style='font-size:smaller;color:gray'>IDE served from device filesystem</p>"));
#else
  WSContentSend_P(PSTR(
    "<hr><h3>TinyC IDE</h3>"
    "<p><input id='ide_url' value='http://localhost:8080' style='width:260px;padding:4px'>"
    "<button onclick=\"window.open(document.getElementById('ide_url').value"
    "+'?device='+location.hostname)\" style='margin-left:8px;padding:4px 12px'>Open IDE</button></p>"
    "<p style='font-size:smaller;color:gray'>IDE URL is saved in browser</p>"
    "<script>var u=localStorage.getItem('tinyc_ide_url');"
    "if(u)document.getElementById('ide_url').value=u;"
    "document.getElementById('ide_url').onchange=function(){"
    "localStorage.setItem('tinyc_ide_url',this.value)};</script>"));
#endif

  WSContentSpaceButton(BUTTON_MAIN);
  WSContentEnd();
}

static void HandleTinyCUploadDone(void) {
  if (!HttpCheckPriviledgedAccess()) { return; }

  // Check if this is an API call (from browser IDE) via ?api=1 query parameter
  bool is_api = Webserver->hasArg("api");

  if (is_api) {
    // JSON response with CORS headers for browser IDE
    Webserver->sendHeader("Access-Control-Allow-Origin", "*");
    Webserver->sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    Webserver->sendHeader("Access-Control-Allow-Headers", "Content-Type");
    if (Tinyc && Tinyc->loaded) {
      char json[128];
      snprintf_P(json, sizeof(json), PSTR("{\"ok\":true,\"size\":%d}"), Tinyc->program_size);
      Webserver->send(200, "application/json", json);
    } else {
      Webserver->send(400, "application/json", "{\"ok\":false,\"error\":\"upload failed\"}");
    }
    return;
  }

  // Regular HTML response for form-based upload from /tc page
  WSContentStart_P(PSTR("TinyC Upload"));
  WSContentSendStyle();

  if (Tinyc && Tinyc->loaded) {
    WSContentSend_P(PSTR("<p style='color:green'><b>Upload successful!</b></p>"
      "<p>Program size: %d bytes</p>"
      "<p><a href='/tc'>Back to TinyC Console</a></p>"),
      Tinyc->program_size);
  } else {
    WSContentSend_P(PSTR("<p style='color:red'><b>Upload failed</b></p>"
      "<p><a href='/tc'>Back to TinyC Console</a></p>"));
  }

  WSContentSpaceButton(BUTTON_MAIN);
  WSContentEnd();
}

// Handle CORS preflight for browser IDE uploads
static void HandleTinyCUploadCORS(void) {
  Webserver->sendHeader("Access-Control-Allow-Origin", "*");
  Webserver->sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
  Webserver->sendHeader("Access-Control-Allow-Headers", "Content-Type");
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
  Webserver->sendHeader("Access-Control-Allow-Origin", "*");
  Webserver->sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  Webserver->sendHeader("Access-Control-Allow-Headers", "Content-Type");

  if (!Tinyc) {
    Webserver->send(500, "application/json", "{\"ok\":false,\"error\":\"not initialized\"}");
    return;
  }

  String cmd = Webserver->arg("cmd");
  char json[256];

  if (cmd == "run") {
    if (!Tinyc->loaded) {
      Webserver->send(400, "application/json", "{\"ok\":false,\"error\":\"no program loaded\"}");
      return;
    }
    if (!TinyCStartVM()) {
      Webserver->send(400, "application/json", "{\"ok\":false,\"error\":\"start failed\"}");
      return;
    }
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: Program started (API)"));
    snprintf_P(json, sizeof(json), PSTR("{\"ok\":true,\"running\":true,\"size\":%d}"), Tinyc->program_size);
    Webserver->send(200, "application/json", json);
  }
  else if (cmd == "stop") {
    TinyCStopVM();
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: Program stopped (API)"));
    Webserver->send(200, "application/json", "{\"ok\":true,\"running\":false}");
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
    Webserver->send(200, "application/json", json);
  }
}

// CORS preflight for /tc_api
static void HandleTinyCApiCORS(void) {
  Webserver->sendHeader("Access-Control-Allow-Origin", "*");
  Webserver->sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  Webserver->sendHeader("Access-Control-Allow-Headers", "Content-Type");
  Webserver->send(204);
}

// ─── Self-hosted IDE (optional — #define USE_TINYC_IDE) ──────
// Serves /tinyc_ide.html (or .gz) from filesystem at /ide
#ifdef USE_TINYC_IDE
#ifdef USE_UFILESYS
static void HandleTinyCIde(void) {
  if (!ffsp) {
    Webserver->send(503, "text/plain", "Filesystem not available");
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
    Webserver->send(404, "text/plain", "TinyC IDE not found. Upload tinyc_ide.html to device filesystem.");
    return;
  }

  uint32_t fsize = f.size();
  if (gzipped) {
    Webserver->sendHeader("Content-Encoding", "gzip");
  }
  Webserver->setContentLength(fsize);
  Webserver->send(200, "text/html", "");

  // Stream file in chunks
  uint8_t buf[512];
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
    ResponseAppend_P(PSTR(",\"TinyC\":{\"Running\":%d,\"Loaded\":%d,\"Size\":%d,\"Instr\":%u}"),
      Tinyc->running ? 1 : 0,
      Tinyc->loaded ? 1 : 0,
      Tinyc->program_size,
      Tinyc->vm.instruction_count);
  }
#ifdef USE_WEBSERVER
  else {
    const char *status = Tinyc->running ? "Running" : (Tinyc->loaded ? "Loaded" : "Empty");
    WSContentSend_PD(PSTR("{s}TinyC{m}%s (%d bytes){e}"),
      status,
      Tinyc->program_size);
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
    case FUNC_EVERY_50_MSECOND:
      TinyCEvery50ms();
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
