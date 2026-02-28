/*
 * Piano Pedal Main Application
 * Ported from ESP32Arduino to ESP-IDF
 *
 * Features:
 * - Three-pedal piano pedal controller (sustain, sostenuto, soft)
 * - Hall sensor calibration
 * - Bluetooth page turner functionality
 * - OTA firmware update via WiFi AP
 * - Buzzer feedback
 * - Power optimization
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_adc_cal.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "driver/ledc.h"
#include "driver/rtc_io.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_attr.h"
#include "esp_pm.h"
#include "esp_task_wdt.h"

#include "config.h"
#include "main.h"
#include "ota_portal.h"

static const char *TAG = "PianoPedal";

/* ========== Global State Variables ========== */
static CalibrationData_t calib_data = {
    .sustain_min = 5000,
    .sustain_max = 0,
    .sostenuto_min = 5000,
    .sostenuto_max = 0,
    .soft_min = 5000,
    .soft_max = 0,
};

static bool in_calibration = false;
static uint64_t calibration_start_ms = 0;
static bool calibration_canceled = false;

static int bluetooth_mode = 0;  // 0: off, 1: BLE MIDI, 2: BLE Keyboard
static bool bluetooth_active = false;

static esp_adc_cal_characteristics_t adc_chars;

/* ========== ADC Helper Functions ========== */
static inline uint32_t read_adc_raw(adc1_channel_t channel)
{
    return adc1_get_raw(channel);
}

static uint32_t read_adc_voltage(adc1_channel_t channel)
{
    uint32_t adc_reading = 0;
    // Take multiple samples for better accuracy
    for (int i = 0; i < 3; i++) {
        adc_reading += adc1_get_raw(channel);
    }
    adc_reading /= 3;
    return esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);
}

/* ========== NVS (Non-Volatile Storage) ========== */
static void nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

static void save_calibration(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("config", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return;
    }

    nvs_set_i32(handle, "sustainmin", calib_data.sustain_min);
    nvs_set_i32(handle, "sustainmax", calib_data.sustain_max);
    nvs_set_i32(handle, "sostenutomin", calib_data.sostenuto_min);
    nvs_set_i32(handle, "sostenutomax", calib_data.sostenuto_max);
    nvs_set_i32(handle, "softmin", calib_data.soft_min);
    nvs_set_i32(handle, "softmax", calib_data.soft_max);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "[SaveCalib] Sustain MIN=%d MAX=%d | Sostenuto MIN=%d MAX=%d | Soft MIN=%d MAX=%d",
             calib_data.sustain_min, calib_data.sustain_max,
             calib_data.sostenuto_min, calib_data.sostenuto_max,
             calib_data.soft_min, calib_data.soft_max);
}

static void read_calibration(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("config", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "NVS not found, using defaults");
        return;
    }

    nvs_get_i32(handle, "sustainmin", &calib_data.sustain_min);
    nvs_get_i32(handle, "sustainmax", &calib_data.sustain_max);
    nvs_get_i32(handle, "sostenutomin", &calib_data.sostenuto_min);
    nvs_get_i32(handle, "sostenutomax", &calib_data.sostenuto_max);
    nvs_get_i32(handle, "softmin", &calib_data.soft_min);
    nvs_get_i32(handle, "softmax", &calib_data.soft_max);
    nvs_close(handle);

    ESP_LOGI(TAG, "[ReadCalib] Sustain MIN=%d MAX=%d | Sostenuto MIN=%d MAX=%d | Soft MIN=%d MAX=%d",
             calib_data.sustain_min, calib_data.sustain_max,
             calib_data.sostenuto_min, calib_data.sostenuto_max,
             calib_data.soft_min, calib_data.soft_max);
}

static void save_bluetooth_active(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("config", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle");
        return;
    }
    nvs_set_u8(handle, "blactive", bluetooth_active ? 1 : 0);
    nvs_commit(handle);
    nvs_close(handle);
}

static void read_bluetooth_active(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("config", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        bluetooth_active = false;
        return;
    }
    uint8_t val = 0;
    nvs_get_u8(handle, "blactive", &val);
    nvs_close(handle);
    bluetooth_active = (val != 0);
}

/* ========== GPIO Configuration ========== */
static void configure_gpio(void)
{
    // Configure button pins with pull-up
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << Sustain_BUTTON_PIN) |
                        (1ULL << Sostenuto_BUTTON_PIN) |
                        (1ULL << Soft_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Configure soft pedal switch output pin
    gpio_config_t switch_conf = {
        .pin_bit_mask = 1ULL << Switch_Soft_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&switch_conf);
    gpio_set_level(Switch_Soft_PIN, 0);
}

/* ========== ADC Configuration ========== */
static void configure_adc(void)
{
    // Configure ADC1
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);   // GPIO35 - Soft
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);   // GPIO32 - Sostenuto
    adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11);   // GPIO33 - Sustain

    // Calibrate ADC
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 
                             ADC_REF_VOLTAGE, &adc_chars);
}

/* ========== DAC Configuration ========== */
static void configure_dac(void)
{
    dac_output_enable(DAC_CHANNEL_1);  // GPIO25 - Sustain
    dac_output_enable(DAC_CHANNEL_2);  // GPIO26 - Sostenuto
}

/* ========== LEDC (PWM) Configuration for Buzzer ========== */
static void configure_buzzer(void)
{
    ledc_timer_config_t timer_conf = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = PWM_FREQ,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = BUZZER_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
    };
    ledc_channel_config(&channel_conf);
}

/* ========== Button Reading ========== */
static bool check_button(int pin)
{
    if (gpio_get_level(pin) == 0) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        if (gpio_get_level(pin) == 0) {
            return true;
        }
    }
    return false;
}

/* ========== Long Press Detection ========== */
static bool check_button_long(int pin, unsigned long hold_ms)
{
    static unsigned long pin_start_times[3] = {0};
    int idx = (pin == Sustain_BUTTON_PIN) ? 0 : (pin == Sostenuto_BUTTON_PIN) ? 1 : 2;
    
    uint32_t current_ms = esp_log_early_timestamp();

    if (gpio_get_level(pin) == 0) {
        if (pin_start_times[idx] == 0) {
            pin_start_times[idx] = current_ms;
        } else if (current_ms - pin_start_times[idx] >= hold_ms) {
            pin_start_times[idx] = 0;
            return true;
        }
    } else {
        pin_start_times[idx] = 0;
    }
    return false;
}

/* ========== Calibration Functions ========== */
static void start_calibration(void)
{
    ESP_LOGI(TAG, "Starting calibration...");
    in_calibration = true;
    calibration_canceled = false;
    calibration_start_ms = esp_log_early_timestamp();

    // Initialize min/max
    calib_data.sustain_min = 5000;
    calib_data.sustain_max = 0;
    calib_data.sostenuto_min = 5000;
    calib_data.sostenuto_max = 0;
    calib_data.soft_min = 5000;
    calib_data.soft_max = 0;

    // Beep indication
    beep_tone(1, 120);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    beep_tone(5, 120);
}

static void finish_calibration(void)
{
    in_calibration = false;
    if (!calibration_canceled) {
        save_calibration();
        beep_tone(5, 240);
        ESP_LOGI(TAG, "Calibration completed, restarting in 3 seconds...");
    } else {
        ESP_LOGI(TAG, "Calibration canceled, not saving results");
    }
    calibration_start_ms = 0;
    calibration_canceled = false;

    vTaskDelay(3000 / portTICK_PERIOD_MS);
    esp_restart();
}

/* ========== ADC Remapping with Smoothing ========== */
static int adc_remap(int pin, int min_v, int max_v, float dead_zone_pct)
{
    // Take multiple samples
    uint32_t raw0 = read_adc_raw(ADC1_CHANNEL_5);
    uint32_t raw1 = read_adc_raw(ADC1_CHANNEL_5);
    uint32_t raw2 = read_adc_raw(ADC1_CHANNEL_5);
    uint32_t adc_value = (raw0 + raw1 + raw2) / 3;
    
    uint32_t adc_voltage = esp_adc_cal_raw_to_voltage(adc_value, &adc_chars);

    if (max_v <= min_v) return 0;

    // Apply dead zone
    float dz = (dead_zone_pct < 0.0f) ? 0.0f : (dead_zone_pct > 0.45f) ? 0.45f : dead_zone_pct;
    
    int remap_min = min_v + (int)((max_v - min_v) * dz);
    int remap_max = max_v - (int)((max_v - min_v) * dz);
    int adc_vol = (adc_voltage < remap_min) ? remap_min : 
                  (adc_voltage > remap_max) ? remap_max : adc_voltage;

    float pct = (float)(adc_vol - remap_min) / (float)(remap_max - remap_min);
    int value_raw = (int)(255 * pct);

    // Smoothing with EMA
    int idx = (pin == ADC_Sustain_PIN) ? 0 : (pin == ADC_Sostenuto_PIN) ? 1 : 2;
    static bool s_inited[3] = {false};
    static float s_ema[3] = {0};
    static int s_last_out[3] = {0};

    if (!s_inited[idx]) {
        s_inited[idx] = true;
        s_ema[idx] = (float)value_raw;
        s_last_out[idx] = value_raw;
    } else {
        float delta = (float)value_raw - s_ema[idx];
        float alpha = (abs((int)delta) > 15) ? 0.7f : 0.2f;
        s_ema[idx] = s_ema[idx] + alpha * delta;

        int ema_int = (int)(s_ema[idx] + (s_ema[idx] >= 0 ? 0.5f : -0.5f));

        if (abs(ema_int - s_last_out[idx]) <= 1) {
            // No change for micro-jitter
        } else {
            const int max_step = 12;
            int step = ema_int - s_last_out[idx];
            if (step > max_step) step = max_step;
            else if (step < -max_step) step = -max_step;
            s_last_out[idx] = s_last_out[idx] + step;
        }
    }

    int value = (s_last_out[idx] < 0) ? 0 : (s_last_out[idx] > 255) ? 255 : s_last_out[idx];

    // Update OTA portal if active
    if (ota_portal_active()) {
        if (pin == ADC_Sustain_PIN) {
            ota_portal_set_pedal_status(2, adc_voltage, min_v, max_v, value);
        } else if (pin == ADC_Sostenuto_PIN) {
            ota_portal_set_pedal_status(1, adc_voltage, min_v, max_v, value);
        } else if (pin == ADC_Soft_PIN) {
            ota_portal_set_pedal_status(0, adc_voltage, min_v, max_v, value);
        }
    }

    return value;
}

/* ========== Buzzer Tone Generation ========== */
static void beep_tone(int degree, int duration_ms)
{
    const uint16_t freqs[7] = {262, 294, 330, 349, 392, 440, 494};
    if (degree < 1 || degree > 7) return;

    uint16_t freq = freqs[degree - 1];
    ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, freq);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 128);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);

    if (duration_ms > 0) {
        vTaskDelay(duration_ms / portTICK_PERIOD_MS);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
    }
}

/* ========== Page Turner Timing ========== */
static unsigned long get_pageturner_continue_time(bool is_down)
{
    static unsigned long down_start_ms = 0;
    static bool downing = false;
    static bool checked = false;

    uint32_t current_ms = esp_log_early_timestamp();

    if (is_down && !downing) {
        down_start_ms = current_ms;
        downing = true;
        checked = false;
    }

    if (!is_down && downing) {
        downing = false;
        if (!checked) {
            return current_ms - down_start_ms;
        }
    }

    if ((current_ms - down_start_ms) >= LongPressTimeMs && downing && !checked) {
        checked = true;
        return LongPressTimeMs;
    }

    return 0;
}

/* ========== Bluetooth Control ========== */
static void shutdown_bluetooth(void)
{
    ESP_LOGI(TAG, "Shutting down Bluetooth");
    esp_bt_controller_disable();
    if (esp_bt_controller_deinit() != ESP_OK) {
        ESP_LOGW(TAG, "Failed to deinit BT controller");
    }
}

/* ========== Hardware Initialization ========== */
static void init_hardware(void)
{
    // Power management configuration
    esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 10,
        .light_sleep_enable = true,
    };
    esp_pm_configure(&pm_config);

    // Task watchdog
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);

    // NVS initialization
    nvs_init();

    // Read calibration data
    read_calibration();
    read_bluetooth_active();

    // GPIO configuration
    configure_gpio();

    // ADC configuration
    configure_adc();

    // DAC configuration
    configure_dac();

    // Buzzer (LEDC) configuration
    configure_buzzer();

    ESP_LOGI(TAG, "Hardware initialized successfully");
}

/* ========== Main Loop ========== */
static void pedal_loop(void)
{
    esp_task_wdt_reset();

    uint32_t loop_start_ms = esp_log_early_timestamp();

    // Calibration mode
    if (in_calibration) {
        uint32_t sus_voltage = read_adc_voltage(ADC1_CHANNEL_5);
        uint32_t sos_voltage = read_adc_voltage(ADC1_CHANNEL_4);
        uint32_t soft_voltage = read_adc_voltage(ADC1_CHANNEL_7);

        // Update min/max
        if (sus_voltage < calib_data.sustain_min)
            calib_data.sustain_min = sus_voltage;
        if (sus_voltage > calib_data.sustain_max)
            calib_data.sustain_max = sus_voltage;

        if (sos_voltage < calib_data.sostenuto_min)
            calib_data.sostenuto_min = sos_voltage;
        if (sos_voltage > calib_data.sostenuto_max)
            calib_data.sostenuto_max = sos_voltage;

        if (soft_voltage < calib_data.soft_min)
            calib_data.soft_min = soft_voltage;
        if (soft_voltage > calib_data.soft_max)
            calib_data.soft_max = soft_voltage;

        uint32_t current_time = esp_log_early_timestamp();
        if (calibration_start_ms != 0 && 
            (current_time - calibration_start_ms >= calibrationTimeoutMs)) {
            in_calibration = false;
            calibration_canceled = true;
            read_calibration();
            ESP_LOGI(TAG, "Calibration timeout: canceled");
            beep_tone(5, 120);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            beep_tone(1, 120);
            calibration_start_ms = 0;
        }

        // Long press to finish calibration
        if (check_button_long(Calibrate_Button, 2000)) {
            finish_calibration();
        }
        return;
    }

    // OTA handling
    if (ota_portal_active()) {
        ota_portal_handle();
    }

    // Read pedal values
    int sustain_value = adc_remap(ADC_Sustain_PIN, 
                                  calib_data.sustain_min, 
                                  calib_data.sustain_max);
    int sostenuto_value = adc_remap(ADC_Sostenuto_PIN,
                                    calib_data.sostenuto_min,
                                    calib_data.sostenuto_max);
    int soft_value = adc_remap(ADC_Soft_PIN,
                               calib_data.soft_min,
                               calib_data.soft_max);

    // Output sustain signal
    dac_output_voltage(DAC_CHANNEL_1, (uint8_t)(sustain_value * Max_DAC_Voltage / 3.3f));

    // Output sostenuto signal (if not connected to BLE keyboard)
    // TODO: Check BLE keyboard connection status
    dac_output_voltage(DAC_CHANNEL_2, (uint8_t)(sostenuto_value * Max_DAC_Voltage / 3.3f));

    // Output soft pedal switch
    gpio_set_level(Switch_Soft_PIN, (soft_value > 127) ? 1 : 0);

    // Page turner functionality
    // TODO: Implement BLE keyboard page control
    if (sostenuto_value > 100) {
        // Pedal down
        unsigned long down_time = get_pageturner_continue_time(true);
        if (down_time == LongPressTimeMs) {
            // Long press - page up
            // TODO: Send KEY_PAGE_UP to BLE
        } else if (down_time > 0 && down_time < LongPressTimeMs) {
            // Short press - page down
            // TODO: Send KEY_PAGE_DOWN to BLE
        }
    } else if (sostenuto_value < 90) {
        get_pageturner_continue_time(false);
    }

    uint32_t loop_ms = esp_log_early_timestamp() - loop_start_ms;
    if (loop_ms < Main_Loop_DelayMs) {
        vTaskDelay((Main_Loop_DelayMs - loop_ms) / portTICK_PERIOD_MS);
    }
}

/* ========== App Main ========== */
void app_main(void)
{
    ESP_LOGI(TAG, "Piano Pedal Controller v%s starting...", FW_VERSION);

    // Initialize hardware
    init_hardware();

    uint32_t sos_voltage = read_adc_voltage(ADC1_CHANNEL_4);
    
    // Check if calibration mode (sustain pedal fully pressed on startup)
    if (sos_voltage > 1500) {
        ESP_LOGI(TAG, "Entering calibration mode...");
        start_calibration();
        
        // Main calibration loop
        while (in_calibration) {
            pedal_loop();
            vTaskDelay(Main_Loop_DelayMs / portTICK_PERIOD_MS);
        }
        return;
    }

    // Check if OTA mode (soft pedal fully pressed on startup)
    int soft_value = adc_remap(ADC_Soft_PIN, calib_data.soft_min, calib_data.soft_max);
    if (soft_value > 127) {
        ESP_LOGI(TAG, "Entering OTA Portal mode...");
        beep_tone(1, 120);
        beep_tone(2, 120);
        beep_tone(3, 120);
        beep_tone(5, 120);
        beep_tone(6, 120);
        
        shutdown_bluetooth();
        vTaskDelay(100 / portTICK_PERIOD_MS);
        ota_portal_begin();

        // OTA loop
        while (true) {
            pedal_loop();
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        return;
    }

    // Normal operation: disable WiFi, keep BLE
    // TODO: Disable WiFi in ESP-IDF
    // esp_wifi_stop();
    // esp_wifi_deinit();

    // Check if toggle Bluetooth (sustain pedal fully pressed at startup)
    int sustain_value = adc_remap(ADC_Sustain_PIN, 
                                  calib_data.sustain_min, 
                                  calib_data.sustain_max);
    if (!ota_portal_active() && sustain_value > 127) {
        bluetooth_active = !bluetooth_active;
        save_bluetooth_active();
        if (bluetooth_active) {
            beep_tone(3, 120);
            beep_tone(5, 120);
            beep_tone(7, 120);
        }
    }

    // TODO: Initialize BLE if needed
    // if (!ota_portal_active() && bluetooth_active) {
    //     ble_keyboard_init();
    // }

    ESP_LOGI(TAG, "Entering normal operation mode...");

    // Main event loop
    while (true) {
        pedal_loop();
        vTaskDelay(Main_Loop_DelayMs / portTICK_PERIOD_MS);
    }
}