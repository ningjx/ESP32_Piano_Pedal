/**
 * @file app_main.c
 * @brief Main Application Entry Point
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_pm.h"
#include "esp_task_wdt.h"

// Import all layer headers
#include "pedal_core.h"
#include "pedal_config.h"
#include "nvs_config.h"
#include "adc_driver.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "ble_keyboard.h"
#include "ble_pageturner.h"

static const char *TAG = "APP";

/**
 * @brief Monitor task - Log pedal states and feed watchdog
 */
static void monitor_task(void *arg)
{
    ESP_LOGI(TAG, "Monitor task started");
    while (1) {
        // Feed watchdog timer
        esp_task_wdt_reset();
        
        pedal_state_t state = pedal_get_state();
        ESP_LOGI(TAG, "Sustain: %d (%dmV) | Sostenuto: %d (%dmV) | Soft: %d (%dmV)",
                 state.sustain_value, state.sustain_mv,
                 state.sostenuto_value, state.sostenuto_mv,
                 state.soft_value, state.soft_mv);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Application main entry point
 */
void app_main(void)
{
    ESP_LOGI(TAG, "===============================================");
    ESP_LOGI(TAG, "  ESP32 Piano Pedal - Modular Architecture v2.0");
    ESP_LOGI(TAG, "===============================================");

    // Power Management: Dynamic frequency scaling to reduce power consumption
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 10,        // Idle frequency
        .light_sleep_enable = true // Enable light sleep mode
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
    ESP_LOGI(TAG, "Power management configured: 80MHz max, 10MHz min, light sleep enabled");

    // Watchdog Timer: Prevent system hang (30 second timeout)
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 30000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,  // All cores
        .trigger_panic = true
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&wdt_config));
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    ESP_LOGI(TAG, "Watchdog timer initialized (30s timeout)");

    // Phase 1: Initialize storage
    ESP_LOGI(TAG, "[1/4] Initializing NVS storage...");
    ESP_ERROR_CHECK(nvs_config_init());

    // Phase 2: Initialize pedal hardware and core
    ESP_LOGI(TAG, "[2/4] Initializing pedal hardware...");
    pedal_init();

    // Phase 3: Load configuration from NVS (optional for now)
    ESP_LOGI(TAG, "[3/4] Loading configuration...");
    pedal_config_t *config = pedal_config_get();
    if (nvs_config_load(config) != ESP_OK) {
        ESP_LOGW(TAG, "No saved configuration found, using defaults");
    }

    // Phase 3.5: Startup mode detection (Arduino compatibility)
    // Wait 50ms for ADC to stabilize
    vTaskDelay(pdMS_TO_TICKS(50));
    
    uint16_t soft_mv = adc_read_pedal_mv(PEDAL_SOFT);
    uint16_t sustain_mv = adc_read_pedal_mv(PEDAL_SUSTAIN);
    uint16_t sostenuto_mv = adc_read_pedal_mv(PEDAL_SOSTENUTO);
    
    ESP_LOGI(TAG, "Startup pedal check: Soft=%dmV Sustain=%dmV Sostenuto=%dmV", 
             soft_mv, sustain_mv, sostenuto_mv);
    
    // OTA Mode: Soft pedal pressed (> 1500mV)
    if (soft_mv > 1500) {
        ESP_LOGW(TAG, "OTA mode detected - Soft pedal pressed (%dmV)", soft_mv);
        ESP_LOGI(TAG, "Starting WiFi SoftAP for OTA update...");
        
        // Shutdown Bluetooth to save memory
        // ble_shutdown(); // TODO: Implement when BLE is ready
        
        // Start WiFi SoftAP
        wifi_init_softap();
        
        // Start Web Server
        web_server_start();
        
        ESP_LOGI(TAG, "OTA portal ready at http://192.168.4.1");
        ESP_LOGI(TAG, "===============================================");
        return; // Skip normal pedal processing
    }
    
    // Bluetooth Mode: Sustain pedal pressed (> 1500mV)
    if (sustain_mv > 1500) {
        ESP_LOGW(TAG, "Bluetooth mode detected - Sustain pedal pressed (%dmV)", sustain_mv);
        config->bluetooth_active = !config->bluetooth_active;
        nvs_config_save(config);
        
        if (config->bluetooth_active) {
            ESP_LOGI(TAG, "Bluetooth enabled - initializing BLE keyboard");
            esp_err_t ret = ble_keyboard_init();
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "BLE keyboard initialized successfully");
                ble_pageturner_start();
                ESP_LOGI(TAG, "Page turner task started - use sostenuto pedal to turn pages");
            } else {
                ESP_LOGE(TAG, "Failed to initialize BLE keyboard");
            }
        } else {
            ESP_LOGI(TAG, "Bluetooth disabled");
            ble_pageturner_stop();
            ble_keyboard_deinit();
        }
    }
    
    // Calibration Mode: Sostenuto pedal pressed (> 1500mV)
    if (sostenuto_mv > 1500) {
        ESP_LOGW(TAG, "Calibration mode detected - Sostenuto pedal pressed (%dmV)", sostenuto_mv);
        pedal_start_calibration();
        ESP_LOGI(TAG, "Calibration mode started - move pedals to min/max positions");
        ESP_LOGI(TAG, "Long press sostenuto button to finish, or wait 20s for auto-cancel");
        ESP_LOGI(TAG, "===============================================");
        return; // Block until calibration completes or cancels
    }

    // Phase 4: Start real-time pedal processing
    ESP_LOGI(TAG, "[4/4] Starting pedal processing...");
    pedal_start_processing();

    // Create monitoring task
    xTaskCreate(monitor_task, "monitor", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "Application initialized successfully!");
    ESP_LOGI(TAG, "===============================================");
}
