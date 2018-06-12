//
//  esp32_ble_beacon.cpp
//  esphomelib
//
//  Created by Otto Winter on 11.06.18.
//  Copyright © 2018 Otto Winter. All rights reserved.
//

#include "esphomelib/esp32_ble_beacon.h"
#include "esphomelib/log.h"

#ifdef USE_ESP32_BLE_BEACON

#include <nvs_flash.h>
#include <freertos/FreeRTOSConfig.h>
#include <esp_bt_main.h>
#include <esp_bt.h>
#include <freertos/task.h>
#include <esp_gap_ble_api.h>
#include "esphomelib/log.h"
#include "esp32_ble_beacon.h"

ESPHOMELIB_NAMESPACE_BEGIN

static const char *TAG = "esp32_ble_beacon";

static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_NONCONN_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr          = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .peer_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))

static esp_ble_ibeacon_head_t ibeacon_common_head = {
    .flags = {0x02, 0x01, 0x06},
    .length = 0x1A,
    .type = 0xFF,
    .company_id = 0x004C,
    .beacon_type = 0x1502
};

void ESP32BLEBeacon::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP32 BLE beacon...");
  global_esp32_ble_beacon = this;

  xTaskCreatePinnedToCore(
      ESP32BLEBeacon::ble_core_task,
      "ble_task", // name
      10000, // stack size (in words)
      nullptr, // input params
      1, // priority
      nullptr, // Handle, not needed
      0 // core
  );
}

float ESP32BLEBeacon::get_setup_priority() const {
  return setup_priority::HARDWARE_LATE;
}
void ESP32BLEBeacon::ble_core_task(void *params) {
// Initialize non-volatile storage for the bluetooth controller
  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init failed: %d", err);
    return;
  }

  // Initialize the bluetooth controller with the default configuration
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  err = esp_bt_controller_init(&bt_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_bt_controller_init failed: %d", err);
    return;
  }

  err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_bt_controller_enable failed: %d", err);
    return;
  }

  err = esp_bluedroid_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_bluedroid_init failed: %d", err);
    return;
  }
  err = esp_bluedroid_enable();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_bluedroid_enable failed: %d", err);
    return;
  }
  err = esp_ble_gap_register_callback(ESP32BLEBeacon::gap_event_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %d", err);
    return;
  }

  esp_ble_ibeacon_t ibeacon_adv_data;
  memcpy(&ibeacon_adv_data.ibeacon_head, &ibeacon_common_head, sizeof(esp_ble_ibeacon_head_t));
  memcpy(&ibeacon_adv_data.ibeacon_vendor.proximity_uuid, global_esp32_ble_beacon->uuid.data(),
         sizeof(ibeacon_adv_data.ibeacon_vendor.proximity_uuid));
  ibeacon_adv_data.ibeacon_vendor.minor = ENDIAN_CHANGE_U16(global_esp32_ble_beacon->minor);
  ibeacon_adv_data.ibeacon_vendor.major = ENDIAN_CHANGE_U16(global_esp32_ble_beacon->major);
  ibeacon_adv_data.ibeacon_vendor.measured_power = 0xC5;

  esp_ble_gap_config_adv_data_raw((uint8_t *) &ibeacon_adv_data, sizeof(ibeacon_adv_data));

  while (true) {
    delay(1000);
  }
}

void ESP32BLEBeacon::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  esp_err_t err;
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
      err = esp_ble_gap_start_advertising(&ble_adv_params);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_start_advertising failed: %d", err);
      }
      break;
    }
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
      err = param->adv_start_cmpl.status;
      if (err != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "BLE adv start failed: %s", esp_err_to_name(err));
      }
      break;
    }
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: {
      err = param->adv_start_cmpl.status;
      if (err != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "BLE adv stop failed: %s", esp_err_to_name(err));
      } else {
        ESP_LOGD(TAG, "BLE stopped advertising successfully");
      }
      break;
    }
    default: break;
  }
}
ESP32BLEBeacon::ESP32BLEBeacon(const std::array<uint8_t, 16> &uuid) : uuid(uuid) {

}
void ESP32BLEBeacon::set_major(uint16_t major) {
  this->major = major;
}
void ESP32BLEBeacon::set_minor(uint16_t minor) {
  this->minor = minor;
}

ESP32BLEBeacon *global_esp32_ble_beacon = nullptr;

ESPHOMELIB_NAMESPACE_END

#endif //USE_ESP32_BLE_BEACON
