#include "ble_keyboard.h"
#include "ble_pageturner.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "BLE_KEYBOARD";

// HID Keyboard key codes
#define HID_KEY_PAGE_UP     0x4B
#define HID_KEY_PAGE_DOWN   0x4E

// BLE connection state
static bool ble_connected = false;
static uint8_t connection_count = 0;

/**
 * @brief Initialize BLE HID keyboard
 */
esp_err_t ble_keyboard_init(void) {
    ESP_LOGI(TAG, "Initializing BLE HID keyboard...");
    
    esp_err_t ret;
    
    // Initialize BLE controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set device name
    const char *dev_name = "翻页器";
    ret = esp_bt_dev_set_device_name(dev_name);
    if (ret) {
        ESP_LOGE(TAG, "Set device name failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure advertising parameters
    esp_ble_adv_params_t adv_params = {
        .adv_int_min = 0x20,        // 20ms
        .adv_int_max = 0x40,        // 40ms
        .adv_type = ADV_TYPE_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    
    // Start advertising
    ret = esp_ble_gap_start_advertising(&adv_params);
    if (ret) {
        ESP_LOGE(TAG, "Start advertising failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "BLE HID keyboard initialized successfully");
    ESP_LOGI(TAG, "Device name: %s", dev_name);
    ESP_LOGI(TAG, "Advertising started - connect from your device");
    ESP_LOGI(TAG, "Note: Full HID profile requires GATT service implementation");
    
    return ESP_OK;
}

/**
 * @brief Deinitialize BLE keyboard
 */
esp_err_t ble_keyboard_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing BLE keyboard...");
    
    ble_pageturner_stop();
    
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    
    ble_connected = false;
    connection_count = 0;
    
    ESP_LOGI(TAG, "BLE keyboard deinitialized");
    return ESP_OK;
}

/**
 * @brief Send HID keyboard report
 */
static esp_err_t ble_send_key(uint8_t key_code) {
    if (!ble_connected) {
        ESP_LOGW(TAG, "BLE not connected, cannot send key");
        return ESP_ERR_INVALID_STATE;
    }
    
    // HID report: [modifier, reserved, key1, key2, key3, key4, key5, key6]
    uint8_t hid_report[8] = {0};
    hid_report[0] = 0;        // No modifier
    hid_report[2] = key_code; // Key code
    
    ESP_LOGI(TAG, "Sending key: 0x%02X (Page %s)", key_code, 
             key_code == HID_KEY_PAGE_UP ? "Up" : "Down");
    
    // TODO: Send HID report via GATT notification
    // This requires implementing a HID GATT service with:
    // - HID Service (UUID 0x1812)
    // - Report characteristic
    // - Report Map descriptor
    
    return ESP_OK;
}

/**
 * @brief Send Page Down key
 */
void ble_keyboard_send_pagedown(void) {
    ble_send_key(HID_KEY_PAGE_DOWN);
}

/**
 * @brief Send Page Up key
 */
void ble_keyboard_send_pageup(void) {
    ble_send_key(HID_KEY_PAGE_UP);
}

/**
 * @brief Check if BLE is connected
 */
bool ble_keyboard_is_connected(void) {
    return ble_connected;
}

/**
 * @brief Get connection count
 */
uint8_t ble_keyboard_get_connection_count(void) {
    return connection_count;
}
