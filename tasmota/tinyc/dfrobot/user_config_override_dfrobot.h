/*
  user_config_override.h — DFRobot FireBeetle 2 ESP32-S3 AI CAM (DFR1154) mit TinyC
  =================================================================================

  Diese Datei gehört nach  tasmota/user_config_override.h  (die Datei steht in der
  .gitignore und kommt darum NICHT mit dem git pull). Sie ist aus gemus eigener
  Konfiguration herausgeschnitten, ohne persönliche Werte: kein WLAN, kein MQTT.
  Anleitung: tasmota/tinyc/dfrobot/ANLEITUNG.md

  Drei Teile:
    1. Der TinyC-Block (-DTINYC_TESTING in der platformio_override.ini schaltet ihn ein)
    2. Die Plugin-Liste (Audio, Matter, ...) — build_plugin.py sucht die Markierungen
       PLUGIN_DEFINES_BEGIN/END und braucht den Block am Dateiende
    3. Der Plugin-Host-Block am Ende (nur für -Dplugin_host_build)
*/

#define MY_LANGUAGE            de_DE
#define LEGACY_GPIO_ARRAY

// Persönliche Werte gibt es hier nicht. Der TinyC-Block unten setzt PERSONAL auf 0
// und leert das WLAN; das Gerät meldet sich nach dem Flashen als Access Point.
#ifndef PERSONAL
#define PERSONAL 0
#endif
#ifndef ROLF
#define ROLF 0
#endif

// TinyC test builds — activated by -DTINYC_TESTING build flag
// ==============================================================
#ifdef TINYC_TESTING

// Disable any active device/feature defines that may interfere with tinyc builds.
// TINYC_TESTING is a GENERIC chip-baseline build (one per chip variant); none
// of the per-device customizations should activate. Add new entries here when
// you uncomment a device_* in the section above and a tinyc* env then breaks.
#undef device_SSD1351
#undef device_RA8876
#undef device_ILI9488
#undef device_ILI9488p16
#undef device_sunton_800_480
#undef device_core2
#undef device_s3box
#undef device_ILI9341_SPI_XPT
#undef device_ILI9341_SPI_XPT2
#undef device_EPD47
#undef device_M5EPD47
#undef device_EPAPER_29
#undef device_EPAPER_29_2
#undef device_EPAPER_42
#undef esp32_7seg
#undef esp32_s3_ili9341p
#undef device_esp32watch
#undef device_bitcoin
#undef device_guition
#undef device_stereo
#undef device_webcam
#undef device_webcam_fe
#undef device_webcam_s3
#undef device_devkit_s3_cam
#undef device_powerwall_relais
#undef device_generic32
#undef device_generic32_c3
#undef device_generic32_rolf
#undef device_esp32_plugins
#undef device_ebus
#undef device_ebus_c3
#undef device_energy
#undef device_ethernet
#undef device_alarm_schalter
#undef device_Solar_Einspeisung
#undef device_Zweirichtungszaehler
#undef device_LEDBAR
#undef device_TVOC2
#undef device_RDM6300
#undef device_CC1101
#undef device_modbus
#undef device_milight
#undef device_magichome1
#undef device_magichome2
#undef device_basic
#undef device_devkit
#undef device_kludi
#undef device_gosund
#undef device_gurtwickler
#undef device_luxmeter
#undef device_lcd
#undef device_rf
#undef device_rfid
#undef device_safeboot
#undef device_scargil
#undef device_scargilh
#undef device_sml_1m
#undef device_sml_4m
#undef device_sonoff_s1
#undef device_sonoff_t2
#undef device_wallbox
#undef device_all_displays
#undef device_ottelo
#undef ROLF

// Also explicitly undef USE_* macros that the active device_* block(s)
// already set BEFORE this TINYC_TESTING block runs (preprocessor is
// top-to-bottom, so #undef'ing the device_* trigger here is too late —
// the chained USE_* defines are already in place). List grows as we
// hit them in CI; current entries cover device_EPD47 leftovers.
#undef USE_LILYGO47
#undef CODE_IMAGE_STR

#undef PERSONAL
#define PERSONAL 0

#undef STREAM_JPEG_PICTS
#undef USE_VL53L0X
#undef USE_TCS34725

#undef MQTT_USER
#define MQTT_USER "user"
#undef MQTT_PASS
#define MQTT_PASS "12345"
// HOME_WIFI (set by the personal env, e.g. tinyc32s3-mini-home) keeps the WLAN creds from
// the PERSONAL block above; without it (the public/release tinyc images for other people)
// the credentials are stripped here. Single source of the creds = the PERSONAL block.
#ifndef HOME_WIFI
#undef STA_SSID1
#define STA_SSID1              ""
#undef  STA_PASS1
#define STA_PASS1              ""
#endif

#undef EMAIL_USER
#define EMAIL_USER "user"
#undef EMAIL_PASSWORD
#define EMAIL_PASSWORD "passwd"
#undef EMAIL_FROM
#define EMAIL_FROM "<mr.x@gmail.com>"
#undef EMAIL_SERVER
#define EMAIL_SERVER "smtp.gmail.com"
#undef EMAIL_PORT
#define EMAIL_PORT 465


#define USE_TINYC
#define USE_TINYC_IDE

#undef SET_ESP32_STACK_SIZE
#define SET_ESP32_STACK_SIZE (12 * 1024)


// Scripter engine — legacy, replaced by TinyC
// Use -DTINYC_NO_SCRIPTER build flag to exclude Scripter (saves ~120 KB flash)
// TinyC fully replaces the Scripter, so drop it on ESP32 too (ESP8266 already
// strips it below "we need all ram here"). Mostly a flash save (~120 KB); the
// RAM saving is small (~0.7 KB static: glob_script_mem + tickers).
#if defined(ESP32) && !defined(TINYC_NO_SCRIPTER)
#define TINYC_NO_SCRIPTER
#endif
#ifdef TINYC_NO_SCRIPTER
#undef USE_SCRIPT
#undef USE_SCRIPT_I2C
#undef USE_SCRIPT_SPI
#undef USE_SCRIPT_SERIAL
#undef USE_SCRIPT_TIMER
#undef USE_SCRIPT_FULL_JSON_PARSER
#undef USE_SCRIPT_WEB_DISPLAY
#undef USE_SCRIPT_JSON_EXPORT
#undef USE_SCRIPT_GLOBVARS
#undef USE_SCRIPT_STATUS
#undef USE_SCRIPT_SUB_COMMAND
#undef USE_SCRIPT_TASK
#undef USE_SCRIPT_FATFS_EXT
#undef USE_SCRIPT_INT
#undef USE_SCRIPT_MDNS
#undef SUPPORT_MQTT_EVENT
#undef USE_FEXTRACT
// TinyC VM needs JsonParsePath — keep it via USE_DT_VARS
#ifndef USE_DT_VARS
#define USE_DT_VARS
#endif
#else
#ifndef USE_SCRIPT
#define USE_SCRIPT
#endif
#define USE_SCRIPT_I2C
#define USE_SCRIPT_SPI
#define USE_SCRIPT_SERIAL
#define USE_SCRIPT_TIMER
#define USE_SCRIPT_FULL_JSON_PARSER
#define USE_SCRIPT_GLOBVARS
#define SUPPORT_MQTT_EVENT
#define USE_FEXTRACT
#define FEXT_MAX_LINE_LENGTH 1024
#endif // TINYC_NO_SCRIPTER

// SML smart meter — independent of Scripter
#define USE_SML_M
#define USE_SML_TCP
// SML scripter-command bridge: enables SML_Write/SML_SetBaud/SML_Read/etc.
// extern declarations consumed by xdrv_124_tinyc_vm.h's MS_OP_SML_BAUD /
// MS_OP_SML_HEX dispatch. Without this, the bytecode opcodes still
// compile (they're gated on USE_SML_M) but the function calls inside
// fail to resolve. Required for examples/sml_ebus.tc and any TinyC
// script that drives a smart meter via the smlWrite() syscall.
#define USE_SML_SCRIPT_CMD

// File system
#define USE_UFILESYS
#undef UFSYS_SIZE
#define UFSYS_SIZE 8192

// Networking
#define USE_WEBCLIENT_HTTPS
#define USE_WEBSEND_RESPONSE

#ifdef ESP32

//#define USE_LIGHT
//#define USE_WS2812

// esp32 include some more
#define USE_TLS
#define USE_SHINE
#define JPEG_PICTS
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32P4)
#undef JPEG_PICTS   // P4: the ES arduino-libs ship no esp32-camera component (esp_camera.h) yet
#endif
#ifndef TINYC_NO_DISPLAY
#define USE_DISPLAY
#define USE_UNIVERSAL_DISPLAY
#define USE_UNIVERSAL_TOUCH
#define USE_TOUCH_BUTTONS
#define DISPDESC_SIZE 2024
#endif
#else
// we need all ram here
#undef USE_SCRIPT
#undef USE_SCRIPT_WEB_DISPLAY
#endif

// I2C + SPI + Timers
#define USE_I2C
#define USE_SPI
#define USE_TIMERS

// Display (optional — enable if tester has a display)
//#define USE_DISPLAY
//#define USE_UNIVERSAL_DISPLAY
//#define USE_UNIVERSAL_TOUCH

#define USE_BINPLUGINS

#undef USE_HOMEKIT
#undef USE_MATTER_C

// Apple HomeKit and Matter share the SAME TinyC integration slot — the
// pairing/QR web page, the "controller wrote a value" callback into the
// VM, and (most of all) the flash budget. They are therefore mutually
// exclusive: pick ONE per build via the env build_flag (-DTINYC_HOMEKIT
// or -DTINYC_MATTER). TINYC_MATTER wins if both are somehow defined.
#if defined(TINYC_MATTER)
  #define USE_MATTER_C
  #ifndef USE_DISCOVERY
    #define USE_DISCOVERY      // Matter on-network commissioning needs mDNS
  #endif
  #ifndef USE_IPV6
    #define USE_IPV6           // Matter MANDATES IPv6 — Apple/Google connect to
  #endif                       // the device over IPv6 (PASE/CASE), not IPv4
#elif defined(TINYC_HOMEKIT)
  #define USE_HOMEKIT
#endif

#ifdef TINYC_CAMERA
#define USE_TINYC_CAMERA
#endif

// Email
#define USE_SENDMAIL

// Tesla Powerwall API access (uses email SSL library)
#define TESLA_POWERWALL
#define SCRIPT_GET_HTTPS_JP

// Disable Berry to save flash (TinyC replaces it)

#undef USE_BERRY

// Enable the on-device LVGL GUI on the display-class TinyC release targets — S3 and P4 —
// unless the env opted out with -DTINYC_NO_LVGL. The S3 release target (tinyc32s3) has no
// device_* block, so without this it would lose LVGL after the device-leak guard; the
// P4-full target also gets USE_TINYC_LVGL from its device_p4_full block, so this is just
// belt-and-suspenders there. Needs lib/libesp32_lvgl in the env's lib_extra_dirs
// (tinyc32s3 / tinyc32-p4-full add it). The lean plain/cam (classic ESP32) and c3/c6
// targets are excluded by the target gate, so they stay LVGL-free.
#if (defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32P4)) && !defined(TINYC_NO_LVGL)
#define USE_LVGL
#define USE_TINYC_LVGL
#endif

// Keep LVGL when a device opted into the TinyC-driven LVGL GUI (USE_TINYC_LVGL);
// the global policy above only meant "no Berry" — LVGL core is Berry-free.
// TINYC_NO_LVGL (set by lean release envs that don't ship the LVGL lib, e.g.
// tinyc32-4M-plain / tinyc32-4M-cam) forces it off — otherwise a USE_TINYC_LVGL
// inherited from the default device's config leaks in and xdrv_54_lvgl tries to
// #include lvgl.h, which isn't in those envs' lib path.
#ifdef TINYC_NO_LVGL
#undef USE_TINYC_LVGL
#endif
#ifndef USE_TINYC_LVGL
#undef USE_LVGL
#endif
#ifdef USE_TINYC_LVGL
// TinyC LVGL = built-in fonts only, no Berry/HASPmota. Disable the Tasmota-level
// LVGL sub-features that xdrv_54 would otherwise call: lv_freetype_init() needs the
// external FreeType lib (LV_USE_FREETYPE is off in tasmota_lv_conf.h), and HASPmota
// is Berry-coupled (lib_ignored). PNG (lodepng) stays — it is internal to lvgl.
#undef USE_LVGL_FREETYPE
#undef USE_LVGL_HASPMOTA
#endif

// ottelos includes
#define USE_COUNTER
#define USE_DEEPSLEEP
#define USE_HOME_ASSISTANT
#define USE_IMPROV
#define USE_LIGHT
#define USE_PING
#define USE_SPI
#define USE_SUNRISE
#define USE_TIMERS
#define USE_TIMERS_WEB
#define USE_WEBSERVER
#define USE_TLS


// --- ESP8266 heap diet (gemu 2026-06-19) ---------------------------------------
// A full-feature TinyC image leaves only ~12 KB free heap on the ESP8266 — too
// tight to load+run even a tiny program (TinyC wants ~16-18 KB). Strip the heavy
// drivers a TinyC smoke-test doesn't need so the VM has room. ESP32 unaffected.
// (Placed after all the USE_* defines above so the #undef's actually stick.)
#ifdef ESP8266
#undef USE_SML_M             // xsns_53 smart-meter / eBUS — large descriptor buffers
#undef USE_SML_TCP
#undef USE_SML_SCRIPT_CMD
#undef USE_BINPLUGINS        // xdrv_123 binary-plugin host
#undef USE_DEEPSLEEP         // xdrv_29
// All displays + touch (the device's LCD_2x20 etc.) — renderer + framebuffer heap
#undef USE_DISPLAY           // master switch: disables support_display + every xdsp_*
#undef USE_UNIVERSAL_DISPLAY
#undef USE_UNIVERSAL_TOUCH
#undef USE_TOUCH_BUTTONS
#undef USE_DISPLAY_LCD
#undef USE_DISPLAY_SSD1306
#undef USE_DISPLAY_SH1106
#undef USE_DISPLAY_MATRIX
#undef USE_DISPLAY_SEVENSEG
#undef USE_DISPLAY_ILI9341
#undef USE_DISPLAY_EPAPER_29
#undef USE_LIGHT             // xdrv_04 light / PWM dimmer
#undef USE_WS2812
#undef USE_IMPROV            // xdrv_62 serial provisioning
#undef USE_PING              // xdrv_38
#undef USE_HOME_ASSISTANT    // xdrv_12 discovery (HA)
#undef USE_TASMOTA_DISCOVERY
#undef USE_MP3_PLAYER        // xdrv_14 DFPlayer
#endif

#undef FRIENDLY_NAME
#define FRIENDLY_NAME        "tinyc"
#undef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID       "tinyc"


#endif  // TINYC_TESTING
// ==============================================================

// >>> PLUGIN_DEFINES_BEGIN <<<  (sentinel — do not edit/remove this line)
// =====================================================================
// Plugin Defines — uncomment ONE to build a Tasmota BinPlugin .bin.
// =====================================================================
// Pair with one of the dedicated plugin envs in platformio_override.ini
// (tasmota-plugin / tasmota32-plugin / tasmota32c3-plugin). Those envs
// pass `-Dplugin_host_build`, which triggers the strip block at the
// END of this file — heavy features (Berry / Zigbee / BLE / KNX /
// Telegram / display drivers / picotts / HomeKit / SML decrypt) get
// undef'd so the plugin compile is fast and lean.
//
// Standalone: works with ANY active `device_*` selector above (or
// none). The plugin envs do not depend on `device_lcd` being set.
//
// Tooling: `python3 tasmota/Plugins/build_plugin.py --plugin <NAME>
// --cpu esp32` (or --gui) toggles exactly one of these for you and
// restores afterward. Edit by hand only if you skip the tool.
//
// Keep at most ONE of the lines below uncommented at a time — Tasmota
// plugins live in dedicated xsns/xdrv slots and can't co-exist in the
// same .bin. The list mirrors `build_plugin.py --list`; new plugin
// .cpp files added under tasmota/Plugins/ should also be listed here.
// =====================================================================

//#define USE_CRC_BLIB_MOD            // xblib_01_crc.cpp
//#define USE_MATTER_MOD              // xblib_02_matter.cpp (matter_c as BLIB — Fork B, stage 1 probe stub)
//#define USE_MATTER_FULL_MOD         // xblib_03_matter_full.cpp (FULL matter_c amalgamation — Fork B, stage 3)
//#define USE_CC1101_MOD              // xdrv_130_cc1101.cpp
//#define USE_MP3_PLAYER_DUAL_MOD     // xdrv_14_mp3_dual.cpp
//#define USE_PCF8574_DUAL_MOD        // xdrv_28_pcf8574_dual.cpp
//#define USE_I2S_MOD                 // xdrv_42_i2s.cpp
//#define USE_DS18X20_DUAL_MOD        // xsns_05_ds18x20_dual.cpp
//#define USE_HTU_DUAL_MOD            // xsns_08_htu21_dual.cpp
//#define USE_BME_DUAL_MOD            // xsns_09_bmp_dual.cpp
//#define USE_LD2410_DUAL_MOD         // xsns_102_ld2410_dual.cpp
//#define USE_TCS34725_DUAL_MOD       // xsns_124_TCS34725_dual.cpp
//#define USE_BRESSER_DUAL_MOD        // xsns_125_bresser_dual.cpp
//#define USE_MORITZ_MOD              // xsns_126_moritz.cpp
//#define USE_ADS1115_DUAL_MOD        // xsns_12_ads1115_dual.cpp
//#define USE_SHT3X_DUAL_MOD          // xsns_14_sht3x_dual.cpp
//#define USE_SHT3X_N2D_MOD           // xsns_198_sht3x_n2d.cpp (native2dual scaffold)
//#define USE_BMP_N2D_MOD             // xsns_198_bmp_n2d.cpp (native2dual scaffold)
//#define USE_PCF8574_N2D_MOD         // xsns_198_pcf8574_n2d.cpp (native2dual scaffold)
//#define USE_MAGICSWITCH_N2D_MOD
//#define USE_TM1638_N2D_MOD
//#define USE_PCA9632_N2D_MOD
//#define USE_DS3502_N2D_MOD
//#define USE_SHIFT595_N2D_MOD
//#define USE_BS814A2_N2D_MOD
//#define USE_FTC532_N2D_MOD
//#define USE_SONOFFD1_N2D_MOD
//#define USE_HOTPLUG_N2D_MOD
//#define USE_ARILUXRF_N2D_MOD
//#define USE_BUZZER_N2D_MOD
//#define USE_FILESETTINGSDEMO_N2D_MOD
//#define USE_FILEJSONSETTINGSDEMO_N2D_MOD
//#define USE_HDC2010_N2D_MOD
//#define USE_LMT01_N2D_MOD
//#define USE_MAX31855_N2D_MOD
//#define USE_LM75AD_N2D_MOD
//#define USE_MGS_N2D_MOD
//#define USE_MAX17043_N2D_MOD
//#define USE_BH1750_N2D_MOD
//#define USE_HMC5883L_N2D_MOD
//#define USE_SNFSC_N2D_MOD
//#define USE_SGP30_DUAL_MOD          // xsns_21_sgp30_dual.cpp
//#define USE_SR04T_DUAL_MOD          // xsns_22_sr04_dual.cpp
//#define USE_CCS811_DUAL_MOD         // xsns_31_ccs811_dual.cpp
//#define USE_PN532_DUAL_MOD          // xsns_40_pn532_dual.cpp
//#define USE_SCD30_DUAL_MOD          // xsns_42_scd30_dual.cpp
//#define USE_SPS30_DUAL_MOD          // xsns_44_sps30_dual.cpp
//#define USE_VL53L0X_DUAL_MOD        // xsns_45_vl53l0x_dual.cpp
//#define USE_MLX90614_DUAL_MOD       // xsns_46_MLX90614_dual.cpp
//#define USE_RDM6300_DUAL_MOD        // xsns_51_rdm6300_dual.cpp
//#define USE_SML_M_MOD               // xsns_53_sml.cpp
//#define USE_VEML6075_DUAL_MOD       // xsns_70_veml6075_dual.cpp
//#define USE_LTR308_DUAL_MOD         // xsns_ltr308_dual.cpp

// =====================================================================
// >>> PLUGIN_DEFINES_END <<<  (sentinel — do not edit/remove this line)



// =====================================================================
// Plugin builds: kill all device_* selectors EARLY.
// =====================================================================
// The *-plugin envs pass `-Dplugin_host_build`. We undef every known
// device_* selector here, BEFORE the device_* selector lines above
// can take effect (preprocessor is top-to-bottom — but the device_*
// `#define` lines have already fired by this point, so this gate
// neutralises them before any `#ifdef device_X … #endif` block fires
// further down). Without this, a setup like `#define device_devkit`
// (the default in this fork) would drag USE_HOMEKIT + extra_dirs
// into the plugin compile and break the link with "hap.h not found".
// =====================================================================
#ifdef plugin_host_build
#undef device_basic
#undef device_devkit
#undef device_devkit_s3_cam
#undef device_lcd
#undef device_ILI9488p16
#undef device_CC1101
#undef device_milight
#undef device_safeboot
#undef device_generic32_rolf
#undef device_sml_1m
#undef device_sml_4m
#undef device32_sml_4m
#undef device_ottelo
#undef device_ethernet
#undef device_ethernet_stick
#undef device_RDM6300
#undef device_LEDBAR
#undef device_TVOC2
#undef device_luxmeter
#undef device_magichome1
#undef device_magichome2
#undef device_modbus
#undef device_Solar_Einspeisung
#undef device_Zweirichtungszaehler
#undef device_ebus
#undef device_ebus_c3
#undef device_gosund
#undef device_sonoff_s1
#undef device_rf
#undef device_all_displays
#undef device_gurtwickler
#undef device_sonoff_t2
#undef device_alarm_schalter
#undef device_wallbox
#undef device_generic
#undef device_generic32
#undef device_generic32_c3
#undef device_esp32_plugins
#undef device_powerwall_relais
#undef device_energy
#undef device_webcam
#undef device_webcam_s3
#undef device_webcam_fe
#undef device_SSD1351
#undef device_RA8876
#undef device_ILI9488
#undef device_sunton_800_480
#undef device_core2
#undef device_s3box
#undef device_ILI9341_SPI_XPT
#undef device_ILI9341_SPI_XPT2
#undef device_EPD47
#undef device_M5EPD47
#undef device_EPAPER_29
#undef device_EPAPER_29_2
#undef device_EPAPER_42
#undef device_esp32watch
#undef device_bitcoin
#undef device_guition
#undef device_stereo
#undef esp32_7seg
#undef esp32_s3_ili9341p
#undef test_dev
#undef test_core1
#undef max_script_bin
#endif  // plugin_host_build


// =====================================================================
// plugin_host_build — strip block (formerly inside `device_lcd`).
// =====================================================================
// Lives at the very end of this file so it fires AFTER all `device_*`
// selector blocks AND the device_ottelo block have done their setup.
// Sits OUTSIDE every conditional gate (no `#ifdef device_*` wrapping)
// so it always processes when -Dplugin_host_build is on the cmd line.
// The 3 plugin envs in platformio_override.ini (tasmota-plugin /
// tasmota32-plugin / tasmota32c3-plugin) pass `-Dplugin_host_build`,
// which engages this block to strip heavy features the plugin
// compile path doesn't need.
//
// The block is fully standalone — does not depend on `device_lcd`,
// `device_devkit`, or any other device_* selector. Anyone forking the
// repo can run `pio run -e tasmota32-plugin` directly after picking a
// plugin in the "Plugin Defines" section near the top.
//
// Stripped (≈100K+ flash, much faster compile):
//   USE_BERRY (+ all USE_BERRY_*), USE_ZIGBEE, USE_BLE_ESP32,
//   USE_MI_ESP32, USE_KNX, USE_TELEGRAM, USE_DOMOTICZ,
//   USE_HOME_ASSISTANT, USE_TASMESH, USE_INFLUXDB, USE_DALI,
//   USE_DISPLAY (+ universal/touch/LVGL), USE_PICOTTS, USE_I2S_AUDIO,
//   USE_WEBCAM, USE_TFT, USE_OPENTHERM, USE_M5STICK_PLUS,
//   USE_GPIO_VIEWER, USE_AUTOCONF, USE_TASMOTA_CLIENT, USE_DEEPSLEEP,
//   USE_DEVICE_GROUPS, USE_TASMOTA_DISCOVERY, USE_HOTPLUG, USE_HOMEKIT.
// Plus `NO_USE_SML_DECRYPT` to suppress AmsLib + bearssl includes
// in xsns_53_sml.cpp (their inline templates don't have the GET_JT
// jumptable setup that the plugin macro-rewrites memcpy/memmove
// to — would yield "'jt' was not declared in this scope" errors).
// Plus `USE_BINPLUGINS` so module.h's descriptor block actually
// compiles (we killed all the device_* selectors that normally
// would have set it).
//
// Intentionally KEPT:
//   USE_TINYC + USE_TINYC_IDE — Arduino preprocessor auto-generates
//   forward decls for every static fn in xdrv_124_tinyc.ino regardless
//   of #ifdef gates. Stripping USE_TINYC leaves those decls without
//   matching definitions → "used but never defined" → build aborts.
//   Easier to leave USE_TINYC enabled (~20-30K extra) than refactor.
//
// The jumptable layout (xdrv_123_plugins.ino's JMPTBL entries) stays
// unconditional, so plugin .bins built here remain ABI-compatible
// with full firmware on the device side.
// =====================================================================
#ifdef plugin_host_build
// Heavy-feature strip
#undef USE_BERRY
#undef USE_BERRY_PYTHON_COMPAT
#undef USE_BERRY_DEBUG
#undef USE_BERRY_TIMING
#undef USE_BERRY_PARTITION_WIZARD
#undef USE_BERRY_WEBCLIENT
#undef USE_BERRY_TLS
#undef USE_BERRY_ULP
#undef USE_BERRY_HOMEKIT
#undef USE_BERRY_GPIOVIEWER
#undef USE_ZIGBEE
#undef USE_ZIGBEE_EZSP
#undef USE_BLE_ESP32
#undef USE_MI_ESP32
#undef USE_TELEGRAM
#undef USE_KNX
#undef USE_DOMOTICZ
#undef USE_HOME_ASSISTANT
#undef USE_TASMESH
#undef USE_INFLUXDB
#undef USE_DALI
#undef USE_DISPLAY
#undef USE_UNIVERSAL_DISPLAY
#undef USE_UNIVERSAL_TOUCH
#undef USE_LVGL
#ifndef TINYC_TTS
#undef USE_PICOTTS
#endif
#undef USE_I2S_AUDIO
#ifdef TINYC_TTS
#undef USE_TINYC_IDE       // omit on-device IDE for the TTS build (frees app + FS)
#endif
#undef USE_WEBCAM
#undef USE_TFT
#undef USE_OPENTHERM
#undef USE_M5STICK_PLUS
#undef USE_GPIO_VIEWER
#undef USE_AUTOCONF
#undef USE_TASMOTA_CLIENT
#undef USE_DEEPSLEEP
#undef USE_DEVICE_GROUPS
#undef USE_TASMOTA_DISCOVERY
#undef USE_HOTPLUG
// HomeKit pulls hap.h from lib/libesp32_div/homekit which the
// tasmota{,32,32c3}-4M envs exclude via lib_extra_dirs — undef at
// top level so the device_devkit (or any other) defaults can't leak.
#undef USE_HOMEKIT
// SML AES-decrypt is opt-OUT in xsns_53_sml.cpp; suppressing it here
// avoids dragging in AmsLib + bearssl headers that have no GET_JT
// setup and break the plugin compile.
#define NO_USE_SML_DECRYPT
// USE_BINPLUGINS gates the real content of tasmota/Plugins/module.h
// (descriptor structs, pFUNC_* selectors, mod_func_execute decl).
// Without it the plugin source can't compile its dispatcher. Normally
// set inside one of the device_* blocks; we kill those above so we
// must define it ourselves here.
#define USE_BINPLUGINS
// USE_TLS pulls in bearssl. xdrv_123_plugins.ino's JMPTBL takes the
// address of br_gcm_* / br_aes_* unconditionally, so the symbols
// must exist. Stripping USE_TELEGRAM / USE_AUTOCONF above removes
// the usual triggers in my_user_config.h's TLS post-process; force
// USE_TLS here so bearssl headers + symbols stay available.
#define USE_TLS
// EXECUTE_FROM_BINARY makes xdrv_123_plugins.ino emit only an
// `extern` declaration of `module_header` instead of its dummy
// definition. Without it, both the plugin host AND the plugin .cpp
// (via MODULE_DESCRIPTOR) define `module_header` — multiple-def
// link error. Also gates the .plugin.mod_part section attribute on
// MODULE_PART decls, which the linker script for plugin builds
// expects.
#define EXECUTE_FROM_BINARY
#endif  // plugin_host_build


// ============================================================================
// TINYC_SHRINK — reusable "fits a small (~1.4MB) app0 partition" profile.
// Enabled by the env build flag -DTINYC_SHRINK (e.g. [env:tinyc32c3-ebus]).
// Placed at global scope AFTER all device_* sections so these #undefs are final
// regardless of which board section was selected. Strips the physical-display +
// heavy optional stacks from an otherwise-full TinyC build for HEADLESS nodes
// (eBUS readers, SML meters, DIN-rail sensors). WebChart / Google Charts is
// browser-side and unaffected; SML + TinyC VM + eBUS master + filesystem stay.
// Use whenever a device's flash partition can't hold the full display image.
// ============================================================================
// The display + graphics stack is the dominant flash cost and is self-contained
// (#ifdef USE_DISPLAY). Mail (USE_ESP32MAIL/SENDMAIL) and MP3 are intentionally
// NOT stripped here: their libs (ESP-Mail-Client BearSSL helper) fail to compile
// when their feature macros vanish but the lib is still pulled in — and they are
// small savings anyway.
#if defined(TINYC_SHRINK) || defined(SML_EBUS_MIN)
#undef USE_DISPLAY
#undef USE_UNIVERSAL_DISPLAY
#undef USE_UNIVERSAL_TOUCH
#undef USE_TOUCH_BUTTONS
#undef USE_LVGL
#undef USE_DISPLAY_LVGL_ONLY
#undef SHOW_SPLASH
#undef JPEG_PICTS
#endif  // TINYC_SHRINK || SML_EBUS_MIN

// SML_EBUS_MIN — minimal "SML + eBUS master + BinPlugins" profile for boards with
// a tiny app partition (e.g. .150 with a 1408KB app0). Also drops the TinyC VM/IDE
// (the largest single block) on top of the display strip above. KEEPS: USE_SML_M +
// USE_SML_EBUS_MASTER, USE_BINPLUGINS (loadable drivers / custom partition), and the
// filesystem — so the board boots, stays reachable, can load BinPlugins, and can be
// repartitioned for a larger app0. WebChart/TinyC scripts won't run on this build.
#ifdef SML_EBUS_MIN
#undef USE_TINYC
#undef USE_TINYC_IDE
#endif  // SML_EBUS_MIN




