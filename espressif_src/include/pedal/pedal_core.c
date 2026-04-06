#include "pedal_core.h"
#include "pedal_config.h"
#include "adc_driver.h"
#include "dac_driver.h"
#include "gpio_driver.h"
#include "nvs_config.h"
#include "ble_keyboard.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "PEDAL_CORE";

static pedal_state_t g_pedal_state = {0};
static TaskHandle_t pedal_task_handle = NULL;
static TaskHandle_t calibration_task_handle = NULL;
static bool in_calibration = false;
static bool calibration_canceled = false;
static TickType_t calibration_start_time = 0;
#define CALIBRATION_TIMEOUT_MS 20000  // 20 seconds timeout

/**
 * @brief 踏板处理任务 (5ms周期)
 */
static void pedal_process_task(void *arg)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(5);

    ESP_LOGI(TAG, "Pedal processing task started");

    while (1) {
        pedal_config_t *config = pedal_config_get();

        // 读取踏板值
        uint8_t sustain_val = adc_read_pedal_value(PEDAL_SUSTAIN);
        uint8_t sostenuto_val = adc_read_pedal_value(PEDAL_SOSTENUTO);
        uint8_t soft_val = adc_read_pedal_value(PEDAL_SOFT);

        uint16_t sustain_mv = adc_read_pedal_mv(PEDAL_SUSTAIN);
        uint16_t sostenuto_mv = adc_read_pedal_mv(PEDAL_SOSTENUTO);
        uint16_t soft_mv = adc_read_pedal_mv(PEDAL_SOFT);

        // 更新状态
        g_pedal_state.sustain_value = sustain_val;
        g_pedal_state.sostenuto_value = sostenuto_val;
        g_pedal_state.soft_value = soft_val;
        g_pedal_state.sustain_mv = sustain_mv;
        g_pedal_state.sostenuto_mv = sostenuto_mv;
        g_pedal_state.soft_mv = soft_mv;

        // 延音踏板处理
        if (sustain_mv >= config->half_pedal_lower_mv &&
            sustain_mv <= config->half_pedal_upper_mv) {
            uint8_t dac_value = (uint8_t)(config->half_pedal_voltage / 3.3f * 255);
            dac_output_sustain(dac_value);
        } else {
            dac_output_sustain(sustain_val);
        }

        // 持音踏板处理
        // 如果蓝牙翻页功能激活且已连接，则不输出持音踏板 DAC 信号
        if (config->bluetooth_active && ble_keyboard_is_connected()) {
            dac_output_sostenuto(0);  // 禁用持音踏板输出
        } else {
            dac_output_sostenuto(sostenuto_val);
        }

        // 弱音踏板处理
        gpio_set_soft_switch(soft_val > 127 ? true : false);

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief 初始化踏板处理
 */
void pedal_init(void)
{
    ESP_LOGI(TAG, "Initializing pedal core...");
    
    // 初始化驱动
    adc_driver_init();
    dac_driver_init();
    
    // 初始化配置管理
    pedal_config_init();

    g_pedal_state.sustain_value = 0;
    g_pedal_state.sostenuto_value = 0;
    g_pedal_state.soft_value = 0;

    ESP_LOGI(TAG, "Pedal core initialized");
}

/**
 * @brief 获取踏板状态
 */
pedal_state_t pedal_get_state(void)
{
    return g_pedal_state;
}

/**
 * @brief 启动踏板处理任务
 */
void pedal_start_processing(void)
{
    if (pedal_task_handle == NULL) {
        xTaskCreate(pedal_process_task, "pedal_task", 4096, NULL, 5, &pedal_task_handle);
        ESP_LOGI(TAG, "Pedal processing task started");
    }
}

/**
 * @brief 停止踏板处理任务
 */
void pedal_stop_processing(void)
{
    if (pedal_task_handle != NULL) {
        vTaskDelete(pedal_task_handle);
        pedal_task_handle = NULL;
        ESP_LOGI(TAG, "Pedal processing task stopped");
    }
}

/**
 * @brief 校准模式任务
 */
static void calibration_task(void *arg)
{
    ESP_LOGI(TAG, "Calibration mode started");
    
    // Initialize min/max values
    pedal_config_t *config = pedal_config_get();
    config->sustain_min_mv = 5000;
    config->sustain_max_mv = 0;
    config->sostenuto_min_mv = 5000;
    config->sostenuto_max_mv = 0;
    config->soft_min_mv = 5000;
    config->soft_max_mv = 0;
    
    // Beep to indicate calibration start (Do-Sol)
    buzzer_play_tone(1, 120);
    buzzer_play_tone(5, 120);
    
    calibration_start_time = xTaskGetTickCount();
    in_calibration = true;
    calibration_canceled = false;
    
    while (in_calibration) {
        // Read current pedal values
        uint16_t sustain_mv = adc_read_pedal_mv(PEDAL_SUSTAIN);
        uint16_t sostenuto_mv = adc_read_pedal_mv(PEDAL_SOSTENUTO);
        uint16_t soft_mv = adc_read_pedal_mv(PEDAL_SOFT);
        
        // Update min/max
        if (sustain_mv < config->sustain_min_mv) config->sustain_min_mv = sustain_mv;
        if (sustain_mv > config->sustain_max_mv) config->sustain_max_mv = sustain_mv;
        if (sostenuto_mv < config->sostenuto_min_mv) config->sostenuto_min_mv = sostenuto_mv;
        if (sostenuto_mv > config->sostenuto_max_mv) config->sostenuto_max_mv = sostenuto_mv;
        if (soft_mv < config->soft_min_mv) config->soft_min_mv = soft_mv;
        if (soft_mv > config->soft_max_mv) config->soft_max_mv = soft_mv;
        
        // Check timeout (20 seconds)
        TickType_t elapsed = (xTaskGetTickCount() - calibration_start_time) * portTICK_PERIOD_MS;
        if (elapsed >= CALIBRATION_TIMEOUT_MS) {
            ESP_LOGW(TAG, "Calibration timeout - canceling");
            pedal_cancel_calibration();
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    calibration_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief 启动校准模式
 */
void pedal_start_calibration(void)
{
    if (calibration_task_handle == NULL) {
        xTaskCreate(calibration_task, "calibration", 4096, NULL, 5, &calibration_task_handle);
    }
}

/**
 * @brief 检查是否在校准模式
 */
bool pedal_is_calibrating(void)
{
    return in_calibration;
}

/**
 * @brief 完成校准并保存
 */
void pedal_finish_calibration(void)
{
    if (!in_calibration) return;
    
    in_calibration = false;
    
    if (!calibration_canceled) {
        // Save calibration data
        pedal_config_t *config = pedal_config_get();
        nvs_config_save(config);
        ESP_LOGI(TAG, "Calibration completed and saved");
        
        // Beep to indicate success (Sol long)
        buzzer_play_tone(5, 240);
        
        // Restart after 3 seconds
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }
}

/**
 * @brief 取消校准
 */
void pedal_cancel_calibration(void)
{
    in_calibration = false;
    calibration_canceled = true;
    
    // Restore previous configuration
    pedal_config_t *config = pedal_config_get();
    nvs_config_load(config);
    
    ESP_LOGI(TAG, "Calibration canceled - restored previous config");
    
    // Beep to indicate cancel (Sol-Do)
    buzzer_play_tone(5, 120);
    buzzer_play_tone(1, 120);
    
    // Restart after 3 seconds
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}
