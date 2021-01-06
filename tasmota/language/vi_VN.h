/*
  vi-VN.h - localization for Vietnam for Tasmota

  Copyright (C) 2021  translateb by Tâm.NT

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _LANGUAGE_VI_VN_H_
#define _LANGUAGE_VI_VN_H_

/*************************** ATTENTION *******************************\
 *
 * Due to memory constraints only UTF-8 is supported.
 * To save code space keep text as short as possible.
 * Time and Date provided by SDK can not be localized (yet).
 * Use online command StateText to translate ON, OFF, HOLD and TOGGLE.
 * Use online command Prefix to translate cmnd, stat and tele.
 *
 * Updated until v9.0.0.1
\*********************************************************************/

//#define LANGUAGE_MODULE_NAME         // Enable to display "Module Generic" (ie Spanish), Disable to display "Generic Module" (ie English)
// https://www.science.co.il/language/Locale-codes.php
#define LANGUAGE_LCID 1066
// HTML (ISO 639-1) Language Code
#define D_HTML_LANGUAGE "vi"

// "2017-03-07T11:08:02" - ISO8601:2004
#define D_YEAR_MONTH_SEPARATOR "-"
#define D_MONTH_DAY_SEPARATOR "-"
#define D_DATE_TIME_SEPARATOR "T"
#define D_HOUR_MINUTE_SEPARATOR ":"
#define D_MINUTE_SECOND_SEPARATOR ":"

#define D_DAY3LIST "CN HaiBa BốnNămSáuBảy"
#define D_MONTH3LIST "JanFebMarAprMayJunJulAugSepOctNovDec"

// Non JSON decimal separator
#define D_DECIMAL_SEPARATOR "."

// Common
#define D_ADMIN "Quản trị"
#define D_AIR_QUALITY "Chất lượng không khí"
#define D_AP "Mạng wifi"                    // Access Point
#define D_AS "với tên gọi"
#define D_AUTO "AUTO"
#define D_BATT "Batt"                // Short for Battery
#define D_BLINK "Blink"
#define D_BLINKOFF "BlinkOff"
#define D_BOOT_COUNT "Số lần khởi động"
#define D_BRIGHTLIGHT "Bright"
#define D_BSSID "BSSId"
#define D_BUTTON "Nút"
#define D_BY "bởi"                    // Written by me
#define D_BYTES "Bytes"
#define D_CELSIUS "Độ C"
#define D_CHANNEL "Kênh"
#define D_CO2 "Khí CO2"
#define D_CODE "code"                // Button code
#define D_COLDLIGHT "Lạnh"
#define D_COMMAND "Dòng Lệnh"
#define D_CONNECTED "Đã kết nối"
#define D_CORS_DOMAIN "CORS Domain"
#define D_COUNT "Đếm"
#define D_COUNTER "Bộ đếm"
#define D_CT_POWER "CT Power"
#define D_CURRENT "Dòng điện"          // As in Voltage and Current
#define D_DATA "Dữ liệu"
#define D_DARKLIGHT "Tối"
#define D_DEBUG "Tìm lỗi"
#define D_DEWPOINT "Điểm sương"
#define D_DISABLED "Vô hiệu hóa"
#define D_DISTANCE "Khoảng cách"
#define D_DNS_SERVER "Máy chủ DNS"
#define D_DO "Disolved Oxygen"
#define D_DONE "Hoàn thành"
#define D_DST_TIME "DST"
#define D_EC "EC"
#define D_ECO2 "eCO2"
#define D_EMULATION "Mô phỏng"
#define D_ENABLED "Kích hoạt"
#define D_ERASE "Xóa"
#define D_ERROR "Lỗi"
#define D_FAHRENHEIT "Độ F"
#define D_FAILED "Thất bại"
#define D_FALLBACK "Fallback"
#define D_FALLBACK_TOPIC "Fallback Topic"
#define D_FALSE "Sai"
#define D_FILE "Tệp"
#define D_FLOW_RATE "Tốc độ dòng"
#define D_FREE_MEMORY "Bộ nhớ trống"
#define D_PSR_MAX_MEMORY "Bộ nhớ PS-RAM"
#define D_PSR_FREE_MEMORY "Bộ nhớ PS-RAM trống"
#define D_FREQUENCY "Tần số"
#define D_GAS "Khí"
#define D_GATEWAY "Cổng kết nối"
#define D_GROUP "Nhóm"
#define D_HOST "Máy chủ"
#define D_HOSTNAME "Tên máy chủ"
#define D_HUMIDITY "Độ ẩm"
#define D_ILLUMINANCE "Độ sáng"
#define D_IMMEDIATE "ngay lập tức"      // Button immediate
#define D_INDEX "Chỉ mục"
#define D_INFO "Thông tin"
#define D_INFRARED "Hồng ngoại"
#define D_INITIALIZED "Khởi tạo"
#define D_IP_ADDRESS "Địa chỉ IP"
#define D_LIGHT "Đèn"
#define D_LWT "LWT"
#define D_LQI "LQI"                  // Zigbee Link Quality Index
#define D_MODULE "Mô đun"
#define D_MOISTURE "Hơi ẩm"
#define D_MQTT "MQTT"
#define D_MULTI_PRESS "bấm nhiều lần"
#define D_NOISE "Nhiễu"
#define D_NONE "Không"
#define D_O2 "Oxygen"
#define D_OFF "Tắt"
#define D_OFFLINE "Ngoại tuyến"
#define D_OK "Ok"
#define D_ON "Bật"
#define D_ONLINE "Trực tuyến"
#define D_ORP "ORP"
#define D_PASSWORD "Mật khẩu"
#define D_PH "pH"
#define D_PORT "Cổng"
#define D_POWER_FACTOR "Hệ số công suất"
#define D_POWERUSAGE "Công suất"
#define D_POWERUSAGE_ACTIVE "Công suất hoạt động"
#define D_POWERUSAGE_APPARENT "Công suất biểu kiến"
#define D_POWERUSAGE_REACTIVE "Công suất phản kháng"
#define D_PRESSURE "Áp suất"
#define D_PRESSUREATSEALEVEL "Mực nước biển"
#define D_PROGRAM_FLASH_SIZE "Kích thước chương trình Flash"
#define D_PROGRAM_SIZE "Kích thước chương trình"
#define D_PROJECT "Dự án"
#define D_RAIN "Mưa"
#define D_RANGE "Khoảng"
#define D_RECEIVED "Đã nhận"
#define D_RESTART "Khởi động lại"
#define D_RESTARTING "Đang khởi động"
#define D_RESTART_REASON "Lý do khởi động lại"
#define D_RESTORE "khôi phục"
#define D_RETAINED "lưu giữ"
#define D_RULE "Quy luật"
#define D_SAVE "Lưu"
#define D_SENSOR "Cảm biến"
#define D_SSID "Tên Wifi"
#define D_START "Bắt đầu"
#define D_STD_TIME "STD"
#define D_STOP "Dừng lại"
#define D_SUBNET_MASK "Subnet Mask"
#define D_SUBSCRIBE_TO "Đăng ký với"
#define D_UNSUBSCRIBE_FROM "Hủy đăng ký từ "
#define D_SUCCESSFUL "Thành công"
#define D_SUNRISE "Mặt trời mọc"
#define D_SUNSET "Mặt trời lặn"
#define D_TEMPERATURE "Nhiệt độ"
#define D_TO "tới"
#define D_TOGGLE "Bật Tắt"
#define D_TOPIC "Chủ đề"
#define D_TOTAL_USAGE "Tổng số đã dùng"
#define D_TRANSMIT "Truyền"
#define D_TRUE "Đúng"
#define D_TVOC "TVOC"
#define D_UPGRADE "nâng cấp"
#define D_UPLOAD "Tải lên"
#define D_UPTIME "Thời gian chạy"
#define D_USER "Người dùng"
#define D_UTC_TIME "UTC"
#define D_UV_INDEX "UV Index"
#define D_UV_INDEX_1 "Thấp"
#define D_UV_INDEX_2 "Trung bình"
#define D_UV_INDEX_3 "Cao"
#define D_UV_INDEX_4 "Nguy hiểm"
#define D_UV_INDEX_5 "BurnL1/2"
#define D_UV_INDEX_6 "BurnL3"
#define D_UV_INDEX_7 "OoR"         // Out of Range
#define D_UV_LEVEL "Mức độ UV"
#define D_UV_POWER "Công suất UV"
#define D_VERSION "Phiên bản"
#define D_VOLTAGE "Điện áp"
#define D_VOLUME "Volume"
#define D_WEIGHT "Cân nặng"
#define D_WARMLIGHT "Ấm"
#define D_WEB_SERVER "Máy chủ Web"

// tasmota.ino
#define D_WARNING_MINIMAL_VERSION "Cảnh báo phiên bản này không hỗ trợ các cài đặt vĩnh viễn"
#define D_LEVEL_10 "level 1-0"
#define D_LEVEL_01 "level 0-1"
#define D_SERIAL_LOGGING_DISABLED "Serial logging disabled"
#define D_SYSLOG_LOGGING_REENABLED "Syslog logging re-enabled"

#define D_SET_BAUDRATE_TO "Đặt Baudrate bằng"
#define D_RECEIVED_TOPIC "Received Topic"
#define D_DATA_SIZE "Kích thước data"
#define D_ANALOG_INPUT "Analog"

// support.ino
#define D_OSWATCH "osWatch"
#define D_BLOCKED_LOOP "Blocked Loop"
#define D_WPS_FAILED_WITH_STATUS "Cấu hình WPS thất bại với trạng thái"
#define D_ACTIVE_FOR_3_MINUTES "Kích hoạt trong 3 phút"
#define D_FAILED_TO_START "khởi động thất bại"
#define D_PATCH_ISSUE_2186 "Bản vá lỗi 2186"
#define D_CONNECTING_TO_AP "Đang kết nối đến điểm truy cập"
#define D_IN_MODE "in mode"
#define D_CONNECT_FAILED_NO_IP_ADDRESS "Kết nối thất bại do không được cấp địa chỉ IP"
#define D_CONNECT_FAILED_AP_NOT_REACHED "Kết nối thất bại do không tìm thấy điểm truy cập"
#define D_CONNECT_FAILED_WRONG_PASSWORD "Kết nối thất bại"
#define D_CONNECT_FAILED_AP_TIMEOUT "Kết nối đến điểm truy cập thất bại do quá thời hạn"
#define D_ATTEMPTING_CONNECTION "Đang thử kết nối..."
#define D_CHECKING_CONNECTION "Đang kiểm tra kết nối..."
#define D_QUERY_DONE "Hoàn thành truy vấn. Đã tìm thấy dịch vụ MQTT"
#define D_MQTT_SERVICE_FOUND "Dịch vụ MQTT đang bật"
#define D_FOUND_AT "tìm thấy tại"
#define D_SYSLOG_HOST_NOT_FOUND "Không tìm thấy máy chủ Syslog"

// settings.ino
#define D_SAVED_TO_FLASH_AT "Lưu vào bộ nhớ flash tại"
#define D_LOADED_FROM_FLASH_AT "Tải từ bộ nhớ flash tại"
#define D_USE_DEFAULTS "Sử dụng mặc định"
#define D_ERASED_SECTOR "Khu vực bị xóa"

// xdrv_02_webserver.ino
#define D_NOSCRIPT "Để sử dụng Tasmota, vui lòng bật JavaScript"
#define D_MINIMAL_FIRMWARE_PLEASE_UPGRADE "Đang sử dụng bản MINIMAL <br>vui lòng nâng cấp"
#define D_WEBSERVER_ACTIVE_ON "Máy chủ Web đã bật"
#define D_WITH_IP_ADDRESS "với địa chỉ IP"
#define D_WEBSERVER_STOPPED "Máy chủ Web đã tắt"
#define D_FILE_NOT_FOUND "Không tìm thấy tệp"
#define D_REDIRECTED "Redirected to captive portal"
#define D_WIFIMANAGER_SET_ACCESSPOINT_AND_STATION "Wifimanager set AccessPoint and keep Station"
#define D_WIFIMANAGER_SET_ACCESSPOINT "Wifimanager set AccessPoint"
#define D_TRYING_TO_CONNECT "Đang thử kết nối mạng cho thiết bị"

#define D_RESTART_IN "Khởi động trong"
#define D_SECONDS "giây"
#define D_DEVICE_WILL_RESTART "Thiết bị sẽ khởi động trong vài giây tới"
#define D_BUTTON_TOGGLE "Bật Tắt"
#define D_CONFIGURATION "Cấu hình"
#define D_INFORMATION "Thông tin"
#define D_FIRMWARE_UPGRADE "Nâng cấp Firmware"
#define D_CONSOLE "Dòng Lệnh"
#define D_CONFIRM_RESTART "Xác nhận khởi động lại"

#define D_CONFIGURE_MODULE "Cấu hình mô đun"
#define D_CONFIGURE_WIFI "Cấu hình WiFi"
#define D_CONFIGURE_MQTT "Cấu hình MQTT"
#define D_CONFIGURE_DOMOTICZ "Cấu hình Domoticz"
#define D_CONFIGURE_LOGGING "Cấu hình ghi chép dữ liệu"
#define D_CONFIGURE_OTHER "Cấu hình khác"
#define D_CONFIRM_RESET_CONFIGURATION "Xác nhận khôi xóa cấu hình"
#define D_RESET_CONFIGURATION "Xóa cấu hình"
#define D_BACKUP_CONFIGURATION "Tạo bản lưu cấu hình"
#define D_RESTORE_CONFIGURATION "Khôi phục cấu hình"
#define D_MAIN_MENU "Màn hình chính"

#define D_MODULE_PARAMETERS "Các thông số mô đun"
#define D_MODULE_TYPE "Loại mô đun"
#define D_PULLUP_ENABLE "Không nút bấm/Công tắc pull-up"
#define D_ADC "ADC"
#define D_GPIO "GPIO"
#define D_SERIAL_IN "Serial In"
#define D_SERIAL_OUT "Serial Out"

#define D_WIFI_PARAMETERS "Thông số Wifi"
#define D_SCAN_FOR_WIFI_NETWORKS "Quét các mạng wifi"
#define D_SCAN_DONE "Đã quét xong"
#define D_NO_NETWORKS_FOUND "Không tìm thấy kết nối mạng"
#define D_REFRESH_TO_SCAN_AGAIN "Quét lại"
#define D_DUPLICATE_ACCESSPOINT "Điểm truy cập trùng lặp"
#define D_SKIPPING_LOW_QUALITY "Bỏ qua do tín hiệu kém"
#define D_RSSI "RSSI"
#define D_WEP "WEP"
#define D_WPA_PSK "WPA PSK"
#define D_WPA2_PSK "WPA2 PSK"
#define D_AP1_SSID "Tên mạng wifi 1"
#define D_AP1_PASSWORD "Mật khẩu mạng wifi 1"
#define D_AP2_SSID "Tên mạng wifi 2"
#define D_AP2_PASSWORD "Mật khẩu mạng wifi 2"

#define D_MQTT_PARAMETERS "Thông số MQTT"
#define D_CLIENT "Client"
#define D_FULL_TOPIC "Full Topic"

#define D_LOGGING_PARAMETERS "Logging parameters"
#define D_SERIAL_LOG_LEVEL "Serial log level"
#define D_MQTT_LOG_LEVEL "Mqtt log level"
#define D_WEB_LOG_LEVEL "Web log level"
#define D_SYS_LOG_LEVEL "Syslog level"
#define D_MORE_DEBUG "More debug"
#define D_SYSLOG_HOST "Syslog host"
#define D_SYSLOG_PORT "Syslog port"
#define D_TELEMETRY_PERIOD "Telemetry period"

#define D_OTHER_PARAMETERS "Các thông số khác"
#define D_TEMPLATE "Mẫu"
#define D_ACTIVATE "Kích hoạt"
#define D_DEVICE_NAME "Tên thiết bị"
#define D_WEB_ADMIN_PASSWORD "Mật khẩu quản trị Web"
#define D_MQTT_ENABLE "Kích hoạt MQTT"
#define D_MQTT_TLS_ENABLE "MQTT TLS"
#define D_FRIENDLY_NAME "Tên hiển thị thiết bị "
#define D_BELKIN_WEMO "Belkin WeMo"
#define D_HUE_BRIDGE "Hue Bridge"
#define D_SINGLE_DEVICE " một thiết bị"
#define D_MULTI_DEVICE "nhiều thiết bị"

#define D_CONFIGURE_TEMPLATE "Cấu hình mẫu"
#define D_TEMPLATE_PARAMETERS "Các thông số mẫu"
#define D_TEMPLATE_NAME "Tên"
#define D_BASE_TYPE "Dựa trên"
#define D_TEMPLATE_FLAGS "Tùy chọn"

#define D_SAVE_CONFIGURATION "Lưu cấu hình"
#define D_CONFIGURATION_SAVED "Cấu hình đã được lưu"
#define D_CONFIGURATION_RESET "Xóa cấu hình"

#define D_PROGRAM_VERSION "Phiên bản trương chình"
#define D_BUILD_DATE_AND_TIME "Ngày giờ tạo"
#define D_CORE_AND_SDK_VERSION "Phiên bản lõi SDK "
#define D_FLASH_WRITE_COUNT "Số lần ghi Flash"
#define D_MAC_ADDRESS "Địa chỉ MAC"
#define D_MQTT_HOST "Máy chủ MQTT"
#define D_MQTT_PORT "Cổng kết nối MQTT"
#define D_MQTT_CLIENT "Máy khách MQTT"
#define D_MQTT_USER "Tên người dùng MQTT"
#define D_MQTT_TOPIC "MQTT Topic"
#define D_MQTT_GROUP_TOPIC "MQTT Group Topic"
#define D_MQTT_FULL_TOPIC "MQTT Full Topic"
#define D_MQTT_NO_RETAIN "Không lưu giữ MQTT"
#define D_MDNS_DISCOVERY "Khám phá mDNS"
#define D_MDNS_ADVERTISE "Thông báo mDNS"
#define D_ESP_CHIP_ID "ESP Chip Id"
#define D_FLASH_CHIP_ID "Flash Chip Id"
#define D_FLASH_CHIP_SIZE "Kích thước bộ nhớ Flash"
#define D_FREE_PROGRAM_SPACE "Bộ nhớ chưa sử dụng"

#define D_UPGRADE_BY_WEBSERVER "Nâng cấp thông qua máy chủ web"
#define D_OTA_URL "Đường dẫn OTA"
#define D_START_UPGRADE "Bắt đầu nâng cấp"
#define D_UPGRADE_BY_FILE_UPLOAD "Nâng cấp thông qua tải lên tệp"
#define D_UPLOAD_STARTED "Bắt đầu tải lên"
#define D_UPGRADE_STARTED "Bắt đầu nâng cấp"
#define D_UPLOAD_DONE "Hoàn thành tải tệp"
#define D_UPLOAD_TRANSFER "Tải lên và truyền tải"
#define D_TRANSFER_STARTED "Bắt đầu truyền tải"
#define D_UPLOAD_ERR_1 "Không có tệp được chọn"
#define D_UPLOAD_ERR_2 "Không đủ bộ nhớ"
#define D_UPLOAD_ERR_3 "Invalid file signature"
#define D_UPLOAD_ERR_4 "Chương trình nâng cấp kích thước lớn hơn bộ nhớ thực tế"
#define D_UPLOAD_ERR_5 "Bộ nhớ đệm tải lên không đủ"
#define D_UPLOAD_ERR_6 "Tải lên thất bại. Bật bản ghi hệ thống mức 3"
#define D_UPLOAD_ERR_7 "Hủy tải lên"
#define D_UPLOAD_ERR_8 "Không đúng loại tệp"
#define D_UPLOAD_ERR_9 "Dung lượng file quá lớn"
#define D_UPLOAD_ERR_10 "Thất bại khi khởi tạo chip RF"
#define D_UPLOAD_ERR_11 "Thất bại khi xóa chip RF"
#define D_UPLOAD_ERR_12 "Thất bại khi ghi lên chip RF"
#define D_UPLOAD_ERR_13 "Thất bại khi giải mã chương trình RF"
#define D_UPLOAD_ERR_14 "Không tương thích"
#define D_UPLOAD_ERROR_CODE "Tải lên bị lỗi mã"

#define D_ENTER_COMMAND "Nhập câu lệnh"
#define D_ENABLE_WEBLOG_FOR_RESPONSE "Kích hoạt bản ghi hệ thống web mức 2 nếu nhận được phản hồi "
#define D_NEED_USER_AND_PASSWORD "Yêu cầu người dùng =<username>&mật khẩu=<password>"

// xdrv_01_mqtt.ino
#define D_FINGERPRINT "Xác nhận thông tin TLS ..."
#define D_TLS_CONNECT_FAILED_TO "Kết nối TLS thất bại đến "
#define D_RETRY_IN "Thử lại trong"
#define D_VERIFIED "Đã xác nhận thông qua vân tay"
#define D_INSECURE "Kết nối không bảo mật do sai thông tin vân tay"
#define D_CONNECT_FAILED_TO "Kết nối thất bại đến"

// xplg_wemohue.ino
#define D_MULTICAST_DISABLED "Vô hiệu hóa Multicast"
#define D_MULTICAST_REJOINED "Đã kết nối lại vào Multicast"
#define D_MULTICAST_JOIN_FAILED "Kết nối Multicast Thất bại"
#define D_FAILED_TO_SEND_RESPONSE "Thất bại khi gửi phản hồi"

#define D_WEMO "WeMo"
#define D_WEMO_BASIC_EVENT "Sự kiên WeMo cơ bản"
#define D_WEMO_EVENT_SERVICE "WeMo event service"
#define D_WEMO_META_SERVICE "WeMo meta service"
#define D_WEMO_SETUP "Cài đặt WeMo"
#define D_RESPONSE_SENT "Gửi phản hồi"

#define D_HUE "Hue"
#define D_HUE_BRIDGE_SETUP "Cài đặt Hue"
#define D_HUE_API_NOT_IMPLEMENTED "Hue API chưa được cung cấp"
#define D_HUE_API "Hue API"
#define D_HUE_POST_ARGS "Hue POST args"
#define D_3_RESPONSE_PACKETS_SENT "gửi 3 gói tin phản hồi"

// xdrv_07_domoticz.ino
#define D_DOMOTICZ_PARAMETERS "Thông số Domoticz"
#define D_DOMOTICZ_IDX "Idx"
#define D_DOMOTICZ_KEY_IDX "Key idx"
#define D_DOMOTICZ_SWITCH_IDX "Switch idx"
#define D_DOMOTICZ_SENSOR_IDX "Sensor idx"
  #define D_DOMOTICZ_TEMP "Temp"
  #define D_DOMOTICZ_TEMP_HUM "Temp,Hum"
  #define D_DOMOTICZ_TEMP_HUM_BARO "Temp,Hum,Baro"
  #define D_DOMOTICZ_POWER_ENERGY "Power,Energy"
  #define D_DOMOTICZ_ILLUMINANCE "Illuminance"
  #define D_DOMOTICZ_COUNT "Count/PM1"
  #define D_DOMOTICZ_VOLTAGE "Voltage/PM2.5"
  #define D_DOMOTICZ_CURRENT "Current/PM10"
  #define D_DOMOTICZ_AIRQUALITY "AirQuality"
  #define D_DOMOTICZ_P1_SMART_METER "P1SmartMeter"
#define D_DOMOTICZ_UPDATE_TIMER "Cập nhật hẹn giờ"

// xdrv_09_timers.ino
#define D_CONFIGURE_TIMER "Cấu hình hẹn giờ"
#define D_TIMER_PARAMETERS "Các thông số hẹn giờ"
#define D_TIMER_ENABLE "Kích hoạt hẹn giờ"
#define D_TIMER_ARM "Kích hoạt"
#define D_TIMER_TIME "Thời gian"
#define D_TIMER_DAYS "Ngày"
#define D_TIMER_REPEAT "Lặp lại"
#define D_TIMER_OUTPUT "Ngõ ra"
#define D_TIMER_ACTION "Hành động"

// xdrv_10_knx.ino
#define D_CONFIGURE_KNX "Cấu hình KNX"
#define D_KNX_PARAMETERS "Thông số KNX "
#define D_KNX_GENERAL_CONFIG "Thông thường"
#define D_KNX_PHYSICAL_ADDRESS "Địa chỉ vật lý"
#define D_KNX_PHYSICAL_ADDRESS_NOTE "( Phải là duy nhât trong mạng KNX)"
#define D_KNX_ENABLE "Enable KNX"
#define D_KNX_GROUP_ADDRESS_TO_WRITE "Dữ liệu gửi đến địa chỉ nhóm"
#define D_ADD "Thêm"
#define D_DELETE "Xóa"
#define D_REPLY "Trả Lời"
#define D_KNX_GROUP_ADDRESS_TO_READ "Địa chỉ nhóm để nhận dữ liệu từ"
#define D_RECEIVED_FROM "Nhận từ"
#define D_KNX_COMMAND_WRITE "Ghi"
#define D_KNX_COMMAND_READ "Đọc"
#define D_KNX_COMMAND_OTHER "Khác"
#define D_SENT_TO "gửi đến"
#define D_KNX_WARNING "Địa chỉ nhóm ( 0 / 0 / 0 ) đã được đặt trước nên không thể sử dụng."
#define D_KNX_ENHANCEMENT "Giao tiếp nâng cao"
#define D_KNX_TX_SLOT "KNX TX"
#define D_KNX_RX_SLOT "KNX RX"
#define D_KNX_TX_SCENE "KNX SCENE TX"
#define D_KNX_RX_SCENE "KNX SCENE RX"

// xdrv_23_zigbee
#define D_ZIGBEE_PERMITJOIN_ACTIVE "Devices allowed to join"
#define D_ZIGBEE_MAPPING_TITLE "Tasmota Zigbee Mapping"
#define D_ZIGBEE_NOT_STARTED "Zigbee not started"
#define D_ZIGBEE_MAPPING_IN_PROGRESS_SEC "Mapping in progress (%d s. remaining)"
#define D_ZIGBEE_MAPPING_NOT_PRESENT "No mapping"
#define D_ZIGBEE_MAP_REFRESH "Zigbee Map Refresh"
#define D_ZIGBEE_MAP   "Zigbee Map"
#define D_ZIGBEE_PERMITJOIN "Zigbee Permit Join"
#define D_ZIGBEE_GENERATE_KEY "generating random Zigbee network key"
#define D_ZIGBEE_UNKNOWN_DEVICE "Unknown device"
#define D_ZIGBEE_UNKNOWN_ATTRIBUTE "Unknown attribute"
#define D_ZIGBEE_INVALID_PARAM "Invalid parameter"
#define D_ZIGBEE_MISSING_PARAM "Missing parameters"
#define D_ZIGBEE_UNKNWON_ATTRIBUTE "Unknown attribute name (ignored): %s"
#define D_ZIGBEE_TOO_MANY_CLUSTERS "No more than one cluster id per command"
#define D_ZIGBEE_WRONG_DELIMITER "Wrong delimiter for payload"
#define D_ZIGBEE_UNRECOGNIZED_COMMAND "Unrecognized zigbee command: %s"
#define D_ZIGBEE_TOO_MANY_COMMANDS "Only 1 command allowed (%d)"
#define D_ZIGBEE_NO_ATTRIBUTE "No attribute in list"
#define D_ZIGBEE_UNSUPPORTED_ATTRIBUTE_TYPE "Unsupported attribute type"
#define D_ZIGBEE_JSON_REQUIRED "Config requires JSON objects"
#define D_ZIGBEE_RESET_1_OR_2 "1 or 2 to reset"
#define D_ZIGBEE_EEPROM_FOUND_AT_ADDRESS "ZBBridge EEPROM found at address"
#define D_ZIGBEE_RANDOMIZING_ZBCONFIG "Randomizing Zigbee parameters, please check with 'ZbConfig'"

// xdrv_03_energy.ino
#define D_ENERGY_TODAY "Năng lượng tiêu thụ hôm nay"
#define D_ENERGY_YESTERDAY "Năng lượng tiêu thụ hôm qua"
#define D_ENERGY_TOTAL "Tổng năng lượng tiêu thụ"

// xdrv_27_shutter.ino
#define D_OPEN "Mở"
#define D_CLOSE "Đóng"
#define D_DOMOTICZ_SHUTTER "Cửa"

// xdrv_28_pcf8574.ino
#define D_CONFIGURE_PCF8574 "Cấu hình PCF8574"
#define D_PCF8574_PARAMETERS "Thông số PCF8574 "
#define D_INVERT_PORTS "Cổng đảo"
#define D_DEVICE "Thiết bị"
#define D_DEVICE_INPUT "Đầu vào"
#define D_DEVICE_OUTPUT "Đầu ra"

// xsns_05_ds18b20.ino
#define D_SENSOR_BUSY "Cảm biến đang bận"
#define D_SENSOR_CRC_ERROR "Cảm biến CRC lỗi"
#define D_SENSORS_FOUND "Tìm thấy cảm biến"

// xsns_06_dht.ino
#define D_TIMEOUT_WAITING_FOR "Timeout khi đợi"
#define D_START_SIGNAL_LOW "bắt đầu tín hiệu thấp"
#define D_START_SIGNAL_HIGH "bắt đầu tín hiệu cao"
#define D_PULSE "xung"
#define D_CHECKSUM_FAILURE "Kiểm tra Checksum thất bại"

// xsns_07_sht1x.ino
#define D_SENSOR_DID_NOT_ACK_COMMAND "Cảm biến không gửi lệnh ACK"
#define D_SHT1X_FOUND "Tìm thấy SHT1X"

// xsns_18_pms5003.ino
#define D_STANDARD_CONCENTRATION "CF-1 PM"     // Standard Particle CF-1 Particle Matter
#define D_ENVIRONMENTAL_CONCENTRATION "PM"     // Environmetal Particle Matter
#define D_PARTICALS_BEYOND "Hạt"

// xsns_27_apds9960.ino
#define D_GESTURE "Cử chỉ"
#define D_COLOR_RED "Đỏ"
#define D_COLOR_GREEN "Xanh lá cây"
#define D_COLOR_BLUE "Xanh da trời"
#define D_CCT "CCT"
#define D_PROXIMITY "Tiệm cận"

// xsns_32_mpu6050.ino
#define D_AX_AXIS "Accel. X-Axis"
#define D_AY_AXIS "Accel. Y-Axis"
#define D_AZ_AXIS "Accel. Z-Axis"
#define D_GX_AXIS "Gyro X-Axis"
#define D_GY_AXIS "Gyro Y-Axis"
#define D_GZ_AXIS "Gyro Z-Axis"

// xsns_34_hx711.ino
#define D_HX_CAL_REMOVE "Remove weigth"
#define D_HX_CAL_REFERENCE "Load reference weigth"
#define D_HX_CAL_DONE "Đã hiệu chỉnh"
#define D_HX_CAL_FAIL "Hiệu chỉnh thất bại"
#define D_RESET_HX711 "Xóa thang chia"
#define D_CONFIGURE_HX711 "Cấu hình thang chia"
#define D_HX711_PARAMETERS "Các thông số thang chia"
#define D_ITEM_WEIGHT "Trọng lượng phần tử"
#define D_REFERENCE_WEIGHT "Trọng lượng tham chiếu"
#define D_CALIBRATE "Tiến hành Hiệu chỉnh"
#define D_CALIBRATION "Hiệu chỉnh"

//xsns_35_tx20.ino
#define D_TX20_WIND_DIRECTION "Hướng gió"
#define D_TX20_WIND_SPEED "Tốc độ gió"
#define D_TX20_WIND_SPEED_MIN "Tốc độ gió tối thiểu"
#define D_TX20_WIND_SPEED_MAX "Tốc độ gió tối đa"
#define D_TX20_NORTH "Bắc"
#define D_TX20_EAST "Đông"
#define D_TX20_SOUTH "Nam"
#define D_TX20_WEST "Tây"

// xsns_53_sml.ino
#define D_TPWRIN "Tổng lượng vào"
#define D_TPWROUT "Tổng lượng ra"
#define D_TPWRCURR "Dòng vào/ra"
#define D_TPWRCURR1 "Dòng vào p1"
#define D_TPWRCURR2 "Dòng vào p2"
#define D_TPWRCURR3 "Dòng vào p3"
#define D_Strom_L1 "Dòng điện L1"
#define D_Strom_L2 "Dòng điện  L2"
#define D_Strom_L3 "Dòng điện L3"
#define D_Spannung_L1 "Điện áp L1"
#define D_Spannung_L2 "Điện áp L2"
#define D_Spannung_L3 "Điện áp L3"
#define D_METERNR "Số đồng hồ đo"
#define D_METERSID "Service ID"
#define D_GasIN "Counter"                // Gas-Verbrauch
#define D_H2oIN "Counter"                // H2o-Verbrauch
#define D_StL1L2L3 "Dòng L1+L2+L3"
#define D_SpL1L2L3 "Điện áp L1+L2+L3/3"

// tasmota_template.h - keep them as short as possible to be able to fit them in GUI drop down box
#define D_SENSOR_NONE          "Không dùng"
#define D_SENSOR_USER          "Người Dùng"
#define D_SENSOR_OPTION        "Option"
#define D_SENSOR_DHT11         "DHT11"
#define D_SENSOR_AM2301        "AM2301"
#define D_SENSOR_SI7021        "SI7021"
#define D_SENSOR_DS18X20       "DS18x20"
#define D_SENSOR_I2C_SCL       "I2C SCL"
#define D_SENSOR_I2C_SDA       "I2C SDA"
#define D_SENSOR_WS2812        "WS2812"
#define D_SENSOR_DFR562        "MP3 Player"
#define D_SENSOR_IRSEND        "IRsend"
#define D_SENSOR_SWITCH        "Switch"     // Suffix "1"
#define D_SENSOR_BUTTON        "Button"     // Suffix "1"
#define D_SENSOR_RELAY         "Relay"      // Suffix "1i"
#define D_SENSOR_LED           "Led"        // Suffix "1i"
#define D_SENSOR_LED_LINK      "LedLink"    // Suffix "i"
#define D_SENSOR_PWM           "PWM"        // Suffix "1"
#define D_SENSOR_COUNTER       "Counter"    // Suffix "1"
#define D_SENSOR_IRRECV        "IRrecv"
#define D_SENSOR_MHZ_RX        "MHZ Rx"
#define D_SENSOR_MHZ_TX        "MHZ Tx"
#define D_SENSOR_PZEM004_RX    "PZEM004 Rx"
#define D_SENSOR_PZEM016_RX    "PZEM016 Rx"
#define D_SENSOR_PZEM017_RX    "PZEM017 Rx"
#define D_SENSOR_PZEM0XX_TX    "PZEM0XX Tx"
#define D_SENSOR_SAIR_RX       "SAir Rx"
#define D_SENSOR_SAIR_TX       "SAir Tx"
#define D_SENSOR_SPI_CS        "SPI CS"
#define D_SENSOR_SPI_DC        "SPI DC"
#define D_SENSOR_SPI_MISO      "SPI MISO"
#define D_SENSOR_SPI_MOSI      "SPI MOSI"
#define D_SENSOR_SPI_CLK       "SPI CLK"
#define D_SENSOR_BACKLIGHT     "Backlight"
#define D_SENSOR_PMS5003_TX    "PMS5003 Tx"
#define D_SENSOR_PMS5003_RX    "PMS5003 Rx"
#define D_SENSOR_SDS0X1_RX     "SDS0X1 Rx"
#define D_SENSOR_SDS0X1_TX     "SDS0X1 Tx"
#define D_SENSOR_HPMA_RX       "HPMA Rx"
#define D_SENSOR_HPMA_TX       "HPMA Tx"
#define D_SENSOR_SBR_RX        "SerBr Rx"
#define D_SENSOR_SBR_TX        "SerBr Tx"
#define D_SENSOR_SR04_TRIG     "SR04 Tri/TX"
#define D_SENSOR_SR04_ECHO     "SR04 Ech/RX"
#define D_SENSOR_SDM120_TX     "SDMx20 Tx"
#define D_SENSOR_SDM120_RX     "SDMx20 Rx"
#define D_SENSOR_SDM630_TX     "SDM630 Tx"
#define D_SENSOR_SDM630_RX     "SDM630 Rx"
#define D_SENSOR_WE517_TX      "WE517 Tx"
#define D_SENSOR_WE517_RX      "WE517 Rx"
#define D_SENSOR_TM1638_CLK    "TM16 CLK"
#define D_SENSOR_TM1638_DIO    "TM16 DIO"
#define D_SENSOR_TM1638_STB    "TM16 STB"
#define D_SENSOR_HX711_SCK     "HX711 SCK"
#define D_SENSOR_HX711_DAT     "HX711 DAT"
#define D_SENSOR_FTC532        "FTC532"
#define D_SENSOR_TX2X_TX       "TX2x"
#define D_SENSOR_RFSEND        "RFSend"
#define D_SENSOR_RFRECV        "RFrecv"
#define D_SENSOR_TUYA_TX       "Tuya Tx"
#define D_SENSOR_TUYA_RX       "Tuya Rx"
#define D_SENSOR_MGC3130_XFER  "MGC3130 Xfr"
#define D_SENSOR_MGC3130_RESET "MGC3130 Rst"
#define D_SENSOR_SSPI_MISO     "SSPI MISO"
#define D_SENSOR_SSPI_MOSI     "SSPI MOSI"
#define D_SENSOR_SSPI_SCLK     "SSPI SCLK"
#define D_SENSOR_SSPI_CS       "SSPI CS"
#define D_SENSOR_SSPI_DC       "SSPI DC"
#define D_SENSOR_RF_SENSOR     "RF Sensor"
#define D_SENSOR_AZ_RX         "AZ Rx"
#define D_SENSOR_AZ_TX         "AZ Tx"
#define D_SENSOR_MAX31855_CS   "MX31855 CS"
#define D_SENSOR_MAX31855_CLK  "MX31855 CLK"
#define D_SENSOR_MAX31855_DO   "MX31855 DO"
#define D_SENSOR_MAX31865_CS   "MX31865 CS"
#define D_SENSOR_NRG_SEL       "HLWBL SEL"  // Suffix "i"
#define D_SENSOR_NRG_CF1       "HLWBL CF1"
#define D_SENSOR_HLW_CF        "HLW8012 CF"
#define D_SENSOR_HJL_CF        "BL0937 CF"
#define D_SENSOR_MCP39F5_TX    "MCP39F5 Tx"
#define D_SENSOR_MCP39F5_RX    "MCP39F5 Rx"
#define D_SENSOR_MCP39F5_RST   "MCP39F5 Rst"
#define D_SENSOR_CSE7766_TX    "CSE7766 Tx"
#define D_SENSOR_CSE7766_RX    "CSE7766 Rx"
#define D_SENSOR_PN532_TX      "PN532 Tx"
#define D_SENSOR_PN532_RX      "PN532 Rx"
#define D_SENSOR_SM16716_CLK   "SM16716 CLK"
#define D_SENSOR_SM16716_DAT   "SM16716 DAT"
#define D_SENSOR_SM16716_POWER "SM16716 PWR"
#define D_SENSOR_P9813_CLK     "P9813 Clk"
#define D_SENSOR_P9813_DAT     "P9813 Dat"
#define D_SENSOR_MY92X1_DI     "MY92x1 DI"
#define D_SENSOR_MY92X1_DCKI   "MY92x1 DCKI"
#define D_SENSOR_ARIRFRCV      "ALux IrRcv"
#define D_SENSOR_ARIRFSEL      "ALux IrSel"
#define D_SENSOR_TXD           "Serial Tx"
#define D_SENSOR_RXD           "Serial Rx"
#define D_SENSOR_ROTARY        "Rotary"     // Suffix "1A"
#define D_SENSOR_HRE_CLOCK     "HRE Clock"
#define D_SENSOR_HRE_DATA      "HRE Data"
#define D_SENSOR_ADE7953_IRQ   "ADE7953 IRQ"
#define D_SENSOR_BUZZER        "Buzzer"
#define D_SENSOR_OLED_RESET    "OLED Reset"
#define D_SENSOR_ZIGBEE_TXD    "Zigbee Tx"
#define D_SENSOR_ZIGBEE_RXD    "Zigbee Rx"
#define D_SENSOR_ZIGBEE_RST    "Zigbee Rst"
#define D_SENSOR_SOLAXX1_TX    "SolaxX1 Tx"
#define D_SENSOR_SOLAXX1_RX    "SolaxX1 Rx"
#define D_SENSOR_IBEACON_TX    "iBeacon TX"
#define D_SENSOR_IBEACON_RX    "iBeacon RX"
#define D_SENSOR_RDM6300_RX    "RDM6300 RX"
#define D_SENSOR_CC1101_CS     "CC1101 CS"
#define D_SENSOR_A4988_DIR     "A4988 DIR"
#define D_SENSOR_A4988_STP     "A4988 STP"
#define D_SENSOR_A4988_ENA     "A4988 ENA"
#define D_SENSOR_A4988_MS1     "A4988 MS1"
#define D_SENSOR_OUTPUT_HI     "Output Hi"
#define D_SENSOR_OUTPUT_LO     "Output Lo"
#define D_SENSOR_AS608_TX      "AS608 Tx"
#define D_SENSOR_AS608_RX      "AS608 Rx"
#define D_SENSOR_DDS2382_TX    "DDS238-2 Tx"
#define D_SENSOR_DDS2382_RX    "DDS238-2 Rx"
#define D_SENSOR_DDSU666_TX    "DDSU666 Tx"
#define D_SENSOR_DDSU666_RX    "DDSU666 Rx"
#define D_SENSOR_SM2135_CLK    "SM2135 Clk"
#define D_SENSOR_SM2135_DAT    "SM2135 Dat"
#define D_SENSOR_DEEPSLEEP     "DeepSleep"
#define D_SENSOR_EXS_ENABLE    "EXS Enable"
#define D_SENSOR_CLIENT_TX      "Client TX"
#define D_SENSOR_CLIENT_RX      "Client RX"
#define D_SENSOR_CLIENT_RESET   "Client RST"
#define D_SENSOR_GPS_RX        "GPS RX"
#define D_SENSOR_GPS_TX        "GPS TX"
#define D_SENSOR_HM10_RX       "HM10 RX"
#define D_SENSOR_HM10_TX       "HM10 TX"
#define D_SENSOR_LE01MR_RX     "LE-01MR Rx"
#define D_SENSOR_LE01MR_TX     "LE-01MR Tx"
#define D_SENSOR_BL0940_RX     "BL0940 Rx"
#define D_SENSOR_CC1101_GDO0   "CC1101 GDO0"
#define D_SENSOR_CC1101_GDO2   "CC1101 GDO2"
#define D_SENSOR_HRXL_RX       "HRXL Rx"
#define D_SENSOR_DYP_RX        "DYP Rx"
#define D_SENSOR_ELECTRIQ_MOODL "MOODL Tx"
#define D_SENSOR_AS3935        "AS3935"
#define D_SENSOR_WINDMETER_SPEED "WindMeter Spd"
#define D_SENSOR_TELEINFO_RX   "TInfo Rx"
#define D_SENSOR_TELEINFO_ENABLE "TInfo EN"
#define D_SENSOR_LMT01_PULSE   "LMT01 Pulse"
#define D_SENSOR_ADC_INPUT     "ADC Input"
#define D_SENSOR_ADC_TEMP      "ADC Temp"
#define D_SENSOR_ADC_LIGHT     "ADC Light"
#define D_SENSOR_ADC_BUTTON    "ADC Button"
#define D_SENSOR_ADC_RANGE     "ADC Range"
#define D_SENSOR_ADC_CT_POWER  "ADC CT Power"
#define D_SENSOR_ADC_JOYSTICK  "ADC Joystick"
#define D_GPIO_WEBCAM_PWDN     "CAM_PWDN"
#define D_GPIO_WEBCAM_RESET    "CAM_RESET"
#define D_GPIO_WEBCAM_XCLK     "CAM_XCLK"
#define D_GPIO_WEBCAM_SIOD     "CAM_SIOD"
#define D_GPIO_WEBCAM_SIOC     "CAM_SIOC"
#define D_GPIO_WEBCAM_DATA     "CAM_DATA"
#define D_GPIO_WEBCAM_VSYNC    "CAM_VSYNC"
#define D_GPIO_WEBCAM_HREF     "CAM_HREF"
#define D_GPIO_WEBCAM_PCLK     "CAM_PCLK"
#define D_GPIO_WEBCAM_PSCLK    "CAM_PSCLK"
#define D_GPIO_WEBCAM_HSD      "CAM_HSD"
#define D_GPIO_WEBCAM_PSRCS    "CAM_PSRCS"
#define D_SENSOR_ETH_PHY_POWER "ETH POWER"
#define D_SENSOR_ETH_PHY_MDC   "ETH MDC"
#define D_SENSOR_ETH_PHY_MDIO  "ETH MDIO"
#define D_SENSOR_TCP_TXD       "TCP Tx"
#define D_SENSOR_TCP_RXD       "TCP Rx"
#define D_SENSOR_IEM3000_TX    "iEM3000 TX"
#define D_SENSOR_IEM3000_RX    "iEM3000 RX"
#define D_SENSOR_MIEL_HVAC_TX  "MiEl HVAC Tx"
#define D_SENSOR_MIEL_HVAC_RX  "MiEl HVAC Rx"
#define D_SENSOR_SHELLY_DIMMER_BOOT0 "SHD Boot 0"
#define D_SENSOR_SHELLY_DIMMER_RST_INV "SHD Reset"
#define D_SENSOR_RC522_RST     "RC522 Rst"
#define D_SENSOR_RC522_CS      "RC522 CS"
#define D_SENSOR_NRF24_CS      "NRF24 CS"
#define D_SENSOR_NRF24_DC      "NRF24 DC"
#define D_SENSOR_ILI9341_CS    "ILI9341 CS"
#define D_SENSOR_ILI9341_DC    "ILI9341 DC"
#define D_SENSOR_ILI9488_CS    "ILI9488 CS"
#define D_SENSOR_EPAPER29_CS   "EPaper29 CS"
#define D_SENSOR_EPAPER42_CS   "EPaper42 CS"
#define D_SENSOR_SSD1351_CS    "SSD1351 CS"
#define D_SENSOR_RA8876_CS     "RA8876 CS"
#define D_SENSOR_ST7789_CS     "ST7789 CS"
#define D_SENSOR_ST7789_DC     "ST7789 DC"
#define D_SENSOR_SSD1331_CS    "SSD1331 CS"
#define D_SENSOR_SSD1331_DC    "SSD1331 DC"
#define D_SENSOR_SDCARD_CS     "SDCard CS"

// Units
#define D_UNIT_AMPERE "A"
#define D_UNIT_CELSIUS "C"
#define D_UNIT_CENTIMETER "cm"
#define D_UNIT_DEGREE "°"
#define D_UNIT_FAHRENHEIT "F"
#define D_UNIT_HERTZ "Hz"
#define D_UNIT_HOUR "h"
#define D_UNIT_GALLONS "gal"
#define D_UNIT_GALLONS_PER_MIN "g/m"
#define D_UNIT_INCREMENTS "inc"
#define D_UNIT_KELVIN "K"
#define D_UNIT_KILOMETER "km"
#define D_UNIT_KILOGRAM "kg"
#define D_UNIT_KILOMETER_PER_HOUR "km/h"  // or "km/h"
#define D_UNIT_KILOOHM "kΩ"
#define D_UNIT_KILOWATTHOUR "kWh"
#define D_UNIT_LITERS "L"
#define D_UNIT_LITERS_PER_MIN "L/m"
#define D_UNIT_LUX "lx"
#define D_UNIT_MICROGRAM_PER_CUBIC_METER "µg/m³"
#define D_UNIT_MICROMETER "µm"
#define D_UNIT_MICROSECOND "µs"
#define D_UNIT_MICROSIEMENS_PER_CM "µS/cm"
#define D_UNIT_MILLIAMPERE "mA"
#define D_UNIT_MILLILITERS "ml"
#define D_UNIT_MILLIMETER "mm"
#define D_UNIT_MILLIMETER_MERCURY "mmHg"
#define D_UNIT_MILLISECOND "ms"
#define D_UNIT_MILLIVOLT "mV"
#define D_UNIT_MINUTE "Min"
#define D_UNIT_PARTS_PER_BILLION "ppb"
#define D_UNIT_PARTS_PER_DECILITER "ppd"
#define D_UNIT_PARTS_PER_MILLION "ppm"
#define D_UNIT_PERCENT "%%"
#define D_UNIT_PRESSURE "hPa"
#define D_UNIT_SECOND "sec"
#define D_UNIT_SECTORS "sectors"
#define D_UNIT_VA "VA"
#define D_UNIT_VAR "VAr"
#define D_UNIT_VOLT "V"
#define D_UNIT_WATT "W"
#define D_UNIT_WATTHOUR "Wh"
#define D_UNIT_WATT_METER_QUADRAT "W/m²"

//SDM220, SDM120, LE01MR
#define D_PHASE_ANGLE     "Góc pha"
#define D_IMPORT_ACTIVE   "Import Active"
#define D_EXPORT_ACTIVE   "Export Active"
#define D_IMPORT_REACTIVE "Import Reactive"
#define D_EXPORT_REACTIVE "Export Reactive"
#define D_TOTAL_REACTIVE  "Total Reactive"
#define D_UNIT_KWARH      "kVArh"
#define D_UNIT_ANGLE      "Deg"
#define D_TOTAL_ACTIVE    "Total Active"

//SOLAXX1
#define D_PV1_VOLTAGE     "Điện áp PV1"
#define D_PV1_CURRENT     "Dòng PV1"
#define D_PV1_POWER       "Nguồn PV1 "
#define D_PV2_VOLTAGE     "Điện áp PV2"
#define D_PV2_CURRENT     "Dòng PV2 "
#define D_PV2_POWER       "Nguồn PV2"
#define D_SOLAR_POWER     "Nguồn năng lượng mặt trời"
#define D_INVERTER_POWER  "Nguồn Inverter"
#define D_STATUS          "Trạng thái"
#define D_WAITING         "Đang chờ"
#define D_CHECKING        "Đang kiểm tra"
#define D_WORKING         "Đang làm việc"
#define D_FAILURE         "Bị lỗi"
#define D_SOLAX_ERROR_0   "Không có mã lỗi"
#define D_SOLAX_ERROR_1   "Lỗi mất lưới điện"
#define D_SOLAX_ERROR_2   "Điện áp điện lưới bị lỗi"
#define D_SOLAX_ERROR_3   "Tần số điện lưới bị lỗi"
#define D_SOLAX_ERROR_4   "Điện áp Pv lỗi "
#define D_SOLAX_ERROR_5   "Lỗi cách ly"
#define D_SOLAX_ERROR_6   "Lỗi quá nhiệt"
#define D_SOLAX_ERROR_7   "Lỗi quạt"
#define D_SOLAX_ERROR_8   "Lỗi thiết bị khác"

//xdrv_10_scripter.ino
#define D_CONFIGURE_SCRIPT     "Chỉnh sửa kịch bản"
#define D_SCRIPT               "chỉnh sửa kịch bản"
#define D_SDCARD_UPLOAD        "tải lên tệp"
#define D_UFSDIR               "thư mục thẻ nhớ ufs"
#define D_UPL_DONE             "Hoàn thành"
#define D_SCRIPT_CHARS_LEFT    "ký tự đã dùng"
#define D_SCRIPT_CHARS_NO_MORE "không còn ký tự trống"
#define D_SCRIPT_DOWNLOAD      "Tải về"
#define D_SCRIPT_ENABLE        "kích hoạt kịch bản"
#define D_SCRIPT_UPLOAD        "Tải lên"
#define D_SCRIPT_UPLOAD_FILES  "Tệp tải lên"

//xsns_67_as3935.ino
#define D_AS3935_GAIN "khuếch đại:"
#define D_AS3935_ENERGY "năng lượng:"
#define D_AS3935_DISTANCE "khoảng cách:"
#define D_AS3935_DISTURBER "nhiễu:"
#define D_AS3935_VRMS "µVrms:"
#define D_AS3935_APRX "aprx.:"
#define D_AS3935_AWAY "xa"
#define D_AS3935_LIGHT "đèn"
#define D_AS3935_OUT "đèn ngoài khoảng xác định"
#define D_AS3935_NOT "không xác định được khoảng cách"
#define D_AS3935_ABOVE "đèn phía trên"
#define D_AS3935_NOISE "phát hiện nhiễu"
#define D_AS3935_DISTDET "phát hiện nhiễu"
#define D_AS3935_INTNOEV "Interrupt with no Event!"
#define D_AS3935_FLICKER "IRQ Pin flicker!"
#define D_AS3935_POWEROFF "Tắt nguồn"
#define D_AS3935_NOMESS "đang lắng nghe..."
#define D_AS3935_ON "Bật"
#define D_AS3935_OFF "Tắt"
#define D_AS3935_INDOORS "Trong nhà"
#define D_AS3935_OUTDOORS "Ngoài trời"
#define D_AS3935_CAL_FAIL "hiệu chỉnh thất bại"
#define D_AS3935_CAL_OK "hiệu chỉnh thiết lập về giá trị:"

//xsns_68_opentherm.ino
#define D_SENSOR_BOILER_OT_RX   "OpenTherm RX"
#define D_SENSOR_BOILER_OT_TX   "OpenTherm TX"

// xnrg_15_teleinfo Denky (Teleinfo)
#define D_CONTRACT        "Hợp đồng"
#define D_POWER_LOAD      "Tải Nguồn "
#define D_CURRENT_TARIFF  "Biểu giá Dòng "
#define D_TARIFF          "Biểu Giá"
#define D_OVERLOAD        "ADPS"
#define D_MAX_POWER       "Nguồn cực đại"
#define D_MAX_CURRENT     "Dòng cực đại"

// xsns_79_as608.ino
#define D_FP_ENROLL_PLACEFINGER "Place finger"
#define D_FP_ENROLL_REMOVEFINGER "Remove finger"
#define D_FP_ENROLL_PLACESAMEFINGER "Place same finger again"
#define D_FP_ENROLL_RETRY "Error so retry"
#define D_FP_ENROLL_RESTART "Restart"
#define D_FP_ENROLL_ERROR "Error"
#define D_FP_ENROLL_RESET "Reset"
#define D_FP_ENROLL_ACTIVE "Active"
#define D_FP_ENROLL_INACTIVE "Inactive"
// Indexed by Adafruit_Fingerprint.h defines
#define D_FP_PACKETRECIEVEERR "Comms error"    // 0x01 Error when receiving data package
#define D_FP_NOFINGER ""                       // 0x02 No finger on the sensor
#define D_FP_IMAGEFAIL "Imaging error"         // 0x03 Failed to enroll the finger
#define D_FP_IMAGEMESS "Image too messy"       // 0x06 Failed to generate character file due to overly disorderly fingerprint image
#define D_FP_FEATUREFAIL "Fingerprint too small" // 0x07 Failed to generate character file due to the lack of character point or small fingerprint image
#define D_FP_NOMATCH "No match"                // 0x08 Finger doesn't match
#define D_FP_NOTFOUND "Did not find a match"   // 0x09 Failed to find matching finger
#define D_FP_ENROLLMISMATCH "Fingerprint did not match" // 0x0A Failed to combine the character files
#define D_FP_BADLOCATION "Bad location"        // 0x0B Addressed PageID is beyond the finger library
#define D_FP_DBRANGEFAIL "DB range error"      // 0x0C Error when reading template from library or invalid template
#define D_FP_UPLOADFEATUREFAIL "Upload feature error" // 0x0D Error when uploading template
#define D_FP_PACKETRESPONSEFAIL "Packet response error" // 0x0E Module failed to receive the following data packages
#define D_FP_UPLOADFAIL "Upload error"         // 0x0F Error when uploading image
#define D_FP_DELETEFAIL "Delete error"         // 0x10 Failed to delete the template
#define D_FP_DBCLEARFAIL "DB Clear error"      // 0x11 Failed to clear finger library
#define D_FP_PASSFAIL "Password error"         // 0x13 Find whether the fingerprint passed or failed
#define D_FP_INVALIDIMAGE "Image invalid"      // 0x15 Failed to generate image because of lac of valid primary image
#define D_FP_FLASHERR "Flash write error"      // 0x18 Error when writing flash
#define D_FP_INVALIDREG "Invalid number"       // 0x1A Invalid register number
#define D_FP_ADDRCODE "Address code"           // 0x20 Address code
#define D_FP_PASSVERIFY "Password verified"    // 0x21 Verify the fingerprint passed
#define D_FP_UNKNOWNERROR "Error"              // Any other error

#endif  // _LANGUAGE_VI_VN_H_
