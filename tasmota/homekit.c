/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
/* HomeKit Smart Outlet Example
*/

#ifdef ESP32

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <driver/gpio.h>


#include <hap.h>

#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#include <hap_platform_keystore.h>

//#include <app_wifi.h>
//#include <app_hap_setup_payload.h>

static const char *TAG = "HAP outlet";
char *hk_desc;
char hk_code[12];


#define SMART_OUTLET_TASK_PRIORITY  1
#define SMART_OUTLET_TASK_STACKSIZE 4 * 1024
#define SMART_OUTLET_TASK_NAME      "hap_outlet"

#define OUTLET_IN_USE_GPIO GPIO_NUM_0
//#define OUTLET_IN_USE_GPIO -1

#define ESP_INTR_FLAG_DEFAULT 0

static xQueueHandle s_esp_evt_queue = NULL;
/**
 * @brief the recover outlet in use gpio interrupt function
 */
static void IRAM_ATTR outlet_in_use_isr(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(s_esp_evt_queue, &gpio_num, NULL);
}

/**
 * Enable a GPIO Pin for Outlet in Use Detection
 */
static void outlet_in_use_key_init(uint32_t key_gpio_pin)
{
    gpio_config_t io_conf;
    /* Interrupt for both the edges  */
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    /* Bit mask of the pins */
    io_conf.pin_bit_mask = 1 << key_gpio_pin;
    /* Set as input mode */
    io_conf.mode = GPIO_MODE_INPUT;
    /* Enable internal pull-up */
    io_conf.pull_up_en = 1;
    /* Disable internal pull-down */
    io_conf.pull_down_en = 0;
    /* Set the GPIO configuration */
    gpio_config(&io_conf);

    /* Install gpio isr service */
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    /* Hook isr handler for specified gpio pin */
    gpio_isr_handler_add(key_gpio_pin, outlet_in_use_isr, (void*)key_gpio_pin);
}

/**
 * Initialize the Smart Outlet Hardware.Here, we just enebale the Outlet-In-Use detection.
 */
void smart_outlet_hardware_init(gpio_num_t gpio_num)
{
  if (gpio_num < 0) return;

    s_esp_evt_queue = xQueueCreate(2, sizeof(uint32_t));
    if (s_esp_evt_queue != NULL) {
        outlet_in_use_key_init(gpio_num);
    }
}


static int bridge_identify(hap_acc_t *ha)
{
    ESP_LOGI(TAG, "Bridge identified");
    return HAP_SUCCESS;
}

static int accessory_identify(hap_acc_t *ha)
{
    hap_serv_t *hs = hap_acc_get_serv_by_uuid(ha, HAP_SERV_UUID_ACCESSORY_INFORMATION);
    hap_char_t *hc = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_NAME);
    const hap_val_t *val = hap_char_get_val(hc);
    char *name = val->s;

    ESP_LOGI(TAG, "Bridged Accessory %s identified", name);
    return HAP_SUCCESS;
}

/* A dummy callback for handling a write on the "On" characteristic of Outlet.
 * In an actual accessory, this should control the hardware
 */
static int outlet_write(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv) {
    int i, ret = HAP_SUCCESS;
    hap_write_data_t *write;
    for (i = 0; i < count; i++) {
        write = &write_data[i];
        if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ON)) {
            //ESP_LOGI(TAG, "Received Write. Outlet %s", write->val.b ? "On" : "Off");
        	ESP_LOG_LEVEL(ESP_LOG_INFO, TAG, "Received Write. Outlet %s", write->val.b ? "On" : "Off");
            /* TODO: Control Actual Hardware */
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
        } else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
        }
    }
    return ret;
}

static int outlet_read(hap_char_t *hc, hap_status_t *status_code, void *serv_priv, void *read_priv)
{
    if (hap_req_get_ctrl_id(read_priv)) {
        ESP_LOGI(TAG, "Received read from %s", hap_req_get_ctrl_id(read_priv));
        printf(" read %s\n", hap_req_get_ctrl_id(read_priv));
    }

    if (!strcmp(hap_char_get_type_uuid(hc), HAP_CHAR_UUID_ON)) {

/*
      hap_char_t *hc = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_FIRMWARE_REVISION);
      const hap_val_t *val = hap_char_get_val(hc);
      uint32_t rev = val->f;
      printf(">>>> rev %d\n", rev);
*/

        hap_val_t new_val;
        new_val.i = 1;
        hap_char_update_val(hc, &new_val);
        *status_code = HAP_STATUS_SUCCESS;
    }
    return HAP_SUCCESS;
}


#define MAX_HAP_DEFS 16
struct HAP_DESC {
  char hap_name[16];
  char var_name[16];
  uint8_t hap_cid;
  hap_acc_t *accessory;
  hap_serv_t *service;
} hap_devs[MAX_HAP_DEFS];

#define HK_SRCBSIZE 256

uint32_t HK_getlinelen(char *lp) {
uint32_t cnt;
  for (cnt=0; cnt<HK_SRCBSIZE-1; cnt++) {
    if (lp[cnt]=='\n') {
      break;
    }
  }
  return cnt;
}


extern void Ext_Replace_Cmd_Vars(char *srcbuf, uint32_t srcsize, char *dstbuf, uint32_t dstsize);


uint32_t str2c(char **sp, char *vp, uint32_t len) {
    char *lp = *sp;
    if (len) len--;
    char *cp = strchr(lp, ',');
    if (cp) {
        while (1) {
            if (*lp == ',') {
                *vp = 0;
                *sp = lp + 1;
                return 0;
            }
            if (len) {
                *vp++ = *lp++;
                len--;
            } else {
                lp++;
            }
        }
    }
    return 1;
}


/*The main thread for handling the Smart Outlet Accessory */
static void smart_outlet_thread_entry(void *p) {
    /* Initialize the HAP core */
    hap_init(HAP_TRANSPORT_WIFI);

    hap_acc_t *accessory;

    hap_acc_cfg_t cfg = {
        .name = "Tasmota-Bridge",
        .manufacturer = "Tasmota",
        .model = "Bridge",
        .serial_num = "001122334455",
        .fw_rev = "0.9.0",
        .hw_rev = NULL,
        .pv = "1.1.0",
        .identify_routine = bridge_identify,
        .cid = HAP_CID_BRIDGE,
    };
    /* Create accessory object */
    accessory = hap_acc_create(&cfg);

    /* Add a dummy Product Data */
    uint8_t product_data[] = {'E','S','P','3','2','H','A','P'};
    hap_acc_add_product_data(accessory, product_data, sizeof(product_data));

    /* Add the Accessory to the HomeKit Database */
    hap_add_accessory(accessory);

    /* Initialise the mandatory parameters for Accessory which will be added as
     * the mandatory services internally
     */
     hap_char_t *outlet_in_use;
    // get values from descriptor, line per line
    // name, cid, var
    char *lp = hk_desc;
    uint8_t index = 0;
    while (*lp) {
      if (*lp == '#') break;
      if (*lp == '\n') lp++;

      char dstbuf[HK_SRCBSIZE*2];
      Ext_Replace_Cmd_Vars(lp, 1, dstbuf, sizeof(dstbuf));
      lp += HK_getlinelen(lp);

      char *lp1 = dstbuf;
      if (str2c(&lp1, hap_devs[index].hap_name, sizeof(hap_devs[index].hap_name))) {
        goto nextline;
      }
      hap_devs[index].hap_cid = strtol(lp1, &lp1, 10);
      if (str2c(&lp1, hap_devs[index].var_name, sizeof(hap_devs[index].var_name))) {
        goto nextline;
      }

      hap_acc_cfg_t hap_cfg;
      hap_cfg.name = hap_devs[index].hap_name;
      hap_cfg.manufacturer = "Tasmota";
      hap_cfg.model = "Tasmota Device";
      hap_cfg.serial_num = "001122334455";
      hap_cfg.fw_rev = "0.9.0";
      hap_cfg.hw_rev = NULL;
      hap_cfg.pv = "1.1.0";
      hap_cfg.identify_routine = accessory_identify;
      hap_cfg.cid =  hap_devs[index].hap_cid;

      /* Create accessory object */
      hap_devs[index].accessory = hap_acc_create(&hap_cfg);
      /* Add a dummy Product Data */
      uint8_t product_data[] = {'E','S','P','3','2','H','A','P'};
      hap_acc_add_product_data(hap_devs[index].accessory, product_data, sizeof(product_data));

      int ret = hap_serv_add_char(hap_devs[index].service, hap_char_name_create(hap_devs[index].hap_name));
      switch (hap_cfg.cid) {
        case HAP_CID_LIGHTING:
          hap_devs[index].service = hap_serv_lightbulb_create(true);
          /* Add the optional characteristic to the Light Bulb Service */
          ret |= hap_serv_add_char(hap_devs[index].service, hap_char_brightness_create(50));
          ret |= hap_serv_add_char(hap_devs[index].service, hap_char_hue_create(180));
          ret |= hap_serv_add_char(hap_devs[index].service, hap_char_saturation_create(100));
          break;
        case HAP_CID_OUTLET:
          hap_devs[index].service = hap_serv_outlet_create(true, true);
          break;
        case HAP_CID_SENSOR:
          hap_devs[index].service = hap_serv_temperature_sensor_create(20);
          break;
        default:
          hap_devs[index].service = hap_serv_outlet_create(true, true);
      }



      /* Get pointer to the outlet in use characteristic which we need to monitor for state changes */
      hap_char_t *outlet_in_use = hap_serv_get_char_by_uuid(hap_devs[index].service, HAP_CHAR_UUID_OUTLET_IN_USE);

      /* Set the write callback for the service */
      hap_serv_set_write_cb(hap_devs[index].service, outlet_write);

      /* Set the write callback for the service */
      hap_serv_set_read_cb(hap_devs[index].service, outlet_read);

      /* Add the Outlet Service to the Accessory Object */
      hap_acc_add_serv(hap_devs[index].accessory, hap_devs[index].service);

      /* Add the Accessory to the HomeKit Database */
      hap_add_bridged_accessory(hap_devs[index].accessory, hap_get_unique_aid(hap_devs[index].hap_name));

      index++;

nextline:
      if (*lp=='\n') {
        lp++;
      } else {
        lp = strchr(lp, '\n');
        if (!lp) break;
        lp++;
      }
    }


    /* Initialize the appliance specific hardware. This enables out-in-use detection */
    smart_outlet_hardware_init(OUTLET_IN_USE_GPIO);

    /* For production accessories, the setup code shouldn't be programmed on to
     * the device. Instead, the setup info, derived from the setup code must
     * be used. Use the factory_nvs_gen utility to generate this data and then
     * flash it into the factory NVS partition.
     *
     * By default, the setup ID and setup info will be read from the factory_nvs
     * Flash partition and so, is not required to set here explicitly.
     *
     * However, for testing purpose, this can be overridden by using hap_set_setup_code()
     * and hap_set_setup_id() APIs, as has been done here.
     */
    hap_set_setup_code(hk_code);
    hap_set_setup_id("ES32");
#ifdef CONFIG_EXAMPLE_USE_HARDCODED_SETUP_CODE
    /* Unique Setup code of the format xxx-xx-xxx. Default: 111-22-333 */
    hap_set_setup_code(CONFIG_EXAMPLE_SETUP_CODE);
    /* Unique four character Setup Id. Default: ES32 */
    hap_set_setup_id(CONFIG_EXAMPLE_SETUP_ID);
#ifdef CONFIG_APP_WIFI_USE_WAC_PROVISIONING
    app_hap_setup_payload(CONFIG_EXAMPLE_SETUP_CODE, CONFIG_EXAMPLE_SETUP_ID, true, hap_devs[0].hap_cid);
#else
    app_hap_setup_payload(CONFIG_EXAMPLE_SETUP_CODE, CONFIG_EXAMPLE_SETUP_ID, false, hap_devs[0].hap_cid);
#endif
#endif // CONFIG_EXAMPLE_USE_HARDCODED_SETUP_CODE

    /* Enable Hardware MFi authentication (applicable only for MFi variant of SDK) */
    hap_enable_mfi_auth(HAP_MFI_AUTH_HW);

    /* Initialize Wi-Fi */
    //app_wifi_init();

    /* After all the initializations are done, start the HAP core */
    hap_start();
    /* Start Wi-Fi */
    //app_wifi_start(portMAX_DELAY);

    uint32_t io_num = OUTLET_IN_USE_GPIO;
    if (io_num >= 0) {
        hap_val_t appliance_value = {
          .b = true,
        };

        /* Listen for Outlet-In-Use state change events. Other read/write functionality will be handled
        * by the HAP Core.
        * When the Outlet in Use GPIO goes low, it means Outlet is not in use.
        * When the Outlet in Use GPIO goes high, it means Outlet is in use.
        * Applications can define own logic as per their hardware.
        */
        while (1) {
          if (xQueueReceive(s_esp_evt_queue, &io_num, portMAX_DELAY) == pdFALSE) {
            ESP_LOGI(TAG, "Outlet-In-Use trigger FAIL");
          } else {
            appliance_value.b = gpio_get_level(io_num);
            /* If any state change is detected, update the Outlet In Use characteristic value */
            hap_char_update_val(outlet_in_use, &appliance_value);
            ESP_LOGI(TAG, "Outlet-In-Use triggered [%d]", appliance_value.b);
          }
        }
    } else {
        while (1) {
        }
    }
}


#define HK_MAXSIZE 1024

void homekit_main(char *desc) {
  if (desc) {
    char *cp = desc;
    cp += 2;
    while (*cp == ' ') cp++;
    // "111-11-111"
    uint32_t cnt;
    for (cnt = 0; cnt < 10; cnt++) {
      hk_code[cnt] = *cp++;
    }
    hk_code[cnt] = 0;
    if (*cp != '\n') {
      printf("init error\n");
      return;
    }
    cp++;
    hk_desc = cp;
  } else {
    hap_platfrom_keystore_erase_partition("nvs");
    return;
  }

  if (!hk_desc) return;

  /* Create the application thread */
  xTaskCreate(smart_outlet_thread_entry, SMART_OUTLET_TASK_NAME, SMART_OUTLET_TASK_STACKSIZE, NULL, SMART_OUTLET_TASK_PRIORITY, NULL);
}

#endif // ESP32
