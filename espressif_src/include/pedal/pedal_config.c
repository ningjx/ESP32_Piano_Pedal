#include "pedal_config.h"
#include "adc_driver.h"
#include "esp_log.h"

static const char *TAG = "PEDAL_CONFIG";

static pedal_config_t g_pedal_config = {
    .sustain_min_mv = 1000,
    .sustain_max_mv = 3200,
    .sostenuto_min_mv = 1000,
    .sostenuto_max_mv = 3200,
    .soft_min_mv = 1000,
    .soft_max_mv = 3200,
    .half_pedal_lower_mv = 1500,
    .half_pedal_upper_mv = 2500,
    .half_pedal_voltage = 1.7f,
    .bluetooth_active = false,
};

/**
 * @brief 初始化配置
 */
void pedal_config_init(void)
{
    ESP_LOGI(TAG, "Pedal config initialized with defaults");
}

/**
 * @brief 获取配置指针
 */
pedal_config_t* pedal_config_get(void)
{
    return &g_pedal_config;
}

/**
 * @brief 设置配置
 */
void pedal_config_set(const pedal_config_t *config)
{
    if (config == NULL) return;

    // 更新ADC校准
    adc_set_calibration(PEDAL_SUSTAIN, config->sustain_min_mv, config->sustain_max_mv);
    adc_set_calibration(PEDAL_SOSTENUTO, config->sostenuto_min_mv, config->sostenuto_max_mv);
    adc_set_calibration(PEDAL_SOFT, config->soft_min_mv, config->soft_max_mv);

    g_pedal_config = *config;
    ESP_LOGI(TAG, "Pedal config updated");
}
