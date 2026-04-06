#include "ble_pageturner.h"
#include "ble_keyboard.h"
#include "pedal_core.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BLE_TURNER";
static TaskHandle_t pageturner_task_handle = NULL;

#define LONG_PRESS_TIME_MS 500
#define PEDAL_THRESHOLD_HIGH 100
#define PEDAL_THRESHOLD_LOW  90

/**
 * @brief Page turner task - monitors sostenuto pedal and sends BLE keys
 */
static void pageturner_task(void *arg) {
    ESP_LOGI(TAG, "Page turner task started");
    
    bool pedal_down = false;
    TickType_t press_start_time = 0;
    bool long_press_sent = false;
    
    while (1) {
        if (!ble_keyboard_is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        // Get sostenuto pedal value
        pedal_state_t state = pedal_get_state();
        uint8_t sostenuto_value = state.sostenuto_value;
        
        // Detect pedal press with hysteresis
        if (!pedal_down && sostenuto_value > PEDAL_THRESHOLD_HIGH) {
            pedal_down = true;
            press_start_time = xTaskGetTickCount();
            long_press_sent = false;
            ESP_LOGD(TAG, "Pedal pressed, value: %d", sostenuto_value);
        } else if (pedal_down && sostenuto_value < PEDAL_THRESHOLD_LOW) {
            // Pedal released
            TickType_t press_duration = (xTaskGetTickCount() - press_start_time) * portTICK_PERIOD_MS;
            
            if (!long_press_sent && press_duration > 0 && press_duration < LONG_PRESS_TIME_MS) {
                // Short press: Page Down
                ESP_LOGI(TAG, "Short press (%d ms) - sending Page Down", press_duration);
                ble_keyboard_send_pagedown();
            }
            
            pedal_down = false;
            ESP_LOGD(TAG, "Pedal released, duration: %d ms", press_duration);
        }
        
        // Check for long press while pedal is still down
        if (pedal_down && !long_press_sent) {
            TickType_t press_duration = (xTaskGetTickCount() - press_start_time) * portTICK_PERIOD_MS;
            if (press_duration >= LONG_PRESS_TIME_MS) {
                // Long press: Page Up
                ESP_LOGI(TAG, "Long press (%d ms) - sending Page Up", press_duration);
                ble_keyboard_send_pageup();
                long_press_sent = true;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms polling interval
    }
}

/**
 * @brief Start page turner task
 */
esp_err_t ble_pageturner_start(void) {
    if (pageturner_task_handle != NULL) {
        ESP_LOGW(TAG, "Page turner task already running");
        return ESP_OK;
    }
    
    BaseType_t ret = xTaskCreate(pageturner_task, "pageturner", 4096, NULL, 5, &pageturner_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create page turner task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Page turner task started successfully");
    return ESP_OK;
}

/**
 * @brief Stop page turner task
 */
esp_err_t ble_pageturner_stop(void) {
    if (pageturner_task_handle == NULL) {
        return ESP_OK;
    }
    
    vTaskDelete(pageturner_task_handle);
    pageturner_task_handle = NULL;
    
    ESP_LOGI(TAG, "Page turner task stopped");
    return ESP_OK;
}
