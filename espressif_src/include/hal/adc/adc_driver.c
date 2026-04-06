#include "adc_driver.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "ADC_DRIVER";

// ADC GPIO 定义 (ESP32)
#define ADC_SUSTAIN_GPIO    35   // ADC1_CH7
#define ADC_SOSTENUTO_GPIO  32   // ADC1_CH4
#define ADC_SOFT_GPIO       33   // ADC1_CH5

// ADC 通道映射
#define ADC_SUSTAIN_CHAN    ADC_CHANNEL_7
#define ADC_SOSTENUTO_CHAN  ADC_CHANNEL_4
#define ADC_SOFT_CHAN       ADC_CHANNEL_5

// 校准范围 (mV) - 默认值
static struct {
    uint16_t min_mv;
    uint16_t max_mv;
    float ema_value;          // EMA smoothing 缓存
    uint8_t last_output;      // 上一次输出值
} pedal_config[3] = {
    {0, 3300, 0.0f, 0},      // Sustain
    {0, 3300, 0.0f, 0},      // Sostenuto
    {0, 3300, 0.0f, 0}       // Soft
};

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;

/**
 * @brief ADC 驱动初始化
 */
void adc_driver_init(void)
{
    ESP_LOGI(TAG, "Initializing ADC driver...");

    // ADC1 单次转换模式配置
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // 配置 ADC1 通道
    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,  // 0-3.6V 范围
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_SUSTAIN_CHAN, &chan_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_SOSTENUTO_CHAN, &chan_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_SOFT_CHAN, &chan_config));

    // ADC 校准初始化
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle));

    // 设置默认校准范围
    pedal_config[PEDAL_SUSTAIN].min_mv = 1000;
    pedal_config[PEDAL_SUSTAIN].max_mv = 3200;
    pedal_config[PEDAL_SOSTENUTO].min_mv = 1000;
    pedal_config[PEDAL_SOSTENUTO].max_mv = 3200;
    pedal_config[PEDAL_SOFT].min_mv = 1000;
    pedal_config[PEDAL_SOFT].max_mv = 3200;

    ESP_LOGI(TAG, "ADC driver initialized");
}

/**
 * @brief 读取 ADC 原始毫伏值 (三次采样平均)
 */
static uint16_t adc_read_raw_mv(adc_channel_t channel)
{
    int raw0, raw1, raw2;
    int adc_raw;
    int adc_mv;

    // 三次快速采样取平均
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, channel, &raw0));
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, channel, &raw1));
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, channel, &raw2));

    adc_raw = (raw0 + raw1 + raw2) / 3;

    // 转换为毫伏
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &adc_mv));

    return (uint16_t)adc_mv;
}

/**
 * @brief 获取相应踏板的 ADC 通道
 */
static adc_channel_t get_adc_channel(pedal_type_t pedal_id)
{
    switch (pedal_id) {
        case PEDAL_SUSTAIN:
            return ADC_SUSTAIN_CHAN;
        case PEDAL_SOSTENUTO:
            return ADC_SOSTENUTO_CHAN;
        case PEDAL_SOFT:
            return ADC_SOFT_CHAN;
        default:
            return ADC_SUSTAIN_CHAN;
    }
}

/**
 * @brief 读取踏板的模拟值 (0-255)
 * 
 * 算法:
 * 1. 三次采样取平均 (去噪)
 * 2. mV 转换
 * 3. 死区处理 (5% 死区)
 * 4. 自适应 EMA 平滑
 * 5. 微步进限幅
 */
uint8_t adc_read_pedal_value(pedal_type_t pedal_id)
{
    uint16_t adc_mv = adc_read_raw_mv(get_adc_channel(pedal_id));

    // 获取校准范围
    uint16_t min_mv = pedal_config[pedal_id].min_mv;
    uint16_t max_mv = pedal_config[pedal_id].max_mv;

    // 死区处理 (5% deadzone)
    float deadzone_pct = 0.05f;
    uint16_t re_min_mv = min_mv + (max_mv - min_mv) * deadzone_pct;
    uint16_t re_max_mv = max_mv - (max_mv - min_mv) * deadzone_pct;

    // 限制在死区范围内
    uint16_t adc_vol = adc_mv;
    if (adc_vol < re_min_mv) adc_vol = re_min_mv;
    if (adc_vol > re_max_mv) adc_vol = re_max_mv;

    // 映射到 0-255
    float pct = (float)(adc_vol - re_min_mv) / (re_max_mv - re_min_mv);
    int value_raw = (int)(255.0f * pct);
    if (value_raw > 255) value_raw = 255;
    if (value_raw < 0) value_raw = 0;

    // 自适应 EMA 平滑 (大变化快速跟随, 小噪声稳定)
    float delta = (float)value_raw - pedal_config[pedal_id].ema_value;
    float alpha = (fabsf(delta) > 15.0f) ? 0.7f : 0.2f;  // 自适应系数
    pedal_config[pedal_id].ema_value += alpha * delta;

    int ema_int = (int)pedal_config[pedal_id].ema_value;

    // 微步进限幅 (最大单步变化 12)
    const int max_step = 12;
    int step = ema_int - pedal_config[pedal_id].last_output;
    if (step > max_step) step = max_step;
    if (step < -max_step) step = -max_step;

    pedal_config[pedal_id].last_output += step;

    if (pedal_config[pedal_id].last_output > 255)
        pedal_config[pedal_id].last_output = 255;
    if (pedal_config[pedal_id].last_output < 0)
        pedal_config[pedal_id].last_output = 0;

    return (uint8_t)pedal_config[pedal_id].last_output;
}

/**
 * @brief 读取踏板的原始毫伏值
 */
uint16_t adc_read_pedal_mv(pedal_type_t pedal_id)
{
    return adc_read_raw_mv(get_adc_channel(pedal_id));
}

/**
 * @brief 设置踏板的校准范围
 */
void adc_set_calibration(pedal_type_t pedal_id, uint16_t min_mv, uint16_t max_mv)
{
    if (pedal_id < 3) {
        pedal_config[pedal_id].min_mv = min_mv;
        pedal_config[pedal_id].max_mv = max_mv;
        ESP_LOGI(TAG, "Pedal %d calibration set: %d-%d mV", pedal_id, min_mv, max_mv);
    }
}

/**
 * @brief 获取踏板的校准范围
 */
void adc_get_calibration(pedal_type_t pedal_id, uint16_t *min_mv, uint16_t *max_mv)
{
    if (pedal_id < 3 && min_mv && max_mv) {
        *min_mv = pedal_config[pedal_id].min_mv;
        *max_mv = pedal_config[pedal_id].max_mv;
    }
}
