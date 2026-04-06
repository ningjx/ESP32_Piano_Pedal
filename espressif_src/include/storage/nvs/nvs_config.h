#ifndef NVS_CONFIG_H
#define NVS_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "pedal_core.h"
#include "esp_err.h"

// NVS Key 定义
#define NVS_KEY_SUSTAIN_MIN     "sustain_min"
#define NVS_KEY_SUSTAIN_MAX     "sustain_max"
#define NVS_KEY_SOSTENUTO_MIN   "sostenuto_min"
#define NVS_KEY_SOSTENUTO_MAX   "sostenuto_max"
#define NVS_KEY_SOFT_MIN        "soft_min"
#define NVS_KEY_SOFT_MAX        "soft_max"
#define NVS_KEY_HALF_LOWER      "half_lower_mv"
#define NVS_KEY_HALF_UPPER      "half_upper_mv"
#define NVS_KEY_HALF_VOLTAGE    "half_voltage"
#define NVS_KEY_BT_ACTIVE       "bt_active"

// NVS 命名空间
#define NVS_NAMESPACE "piano_pedal"

// NVS 初始化和反初始化
esp_err_t nvs_config_init(void);
void nvs_config_deinit(void);

// 加载配置
esp_err_t nvs_config_load(pedal_config_t *config);

// 保存配置
esp_err_t nvs_config_save(const pedal_config_t *config);

// 保存校准数据
esp_err_t nvs_save_calibration(const pedal_config_t *config);

// 加载校准数据
esp_err_t nvs_load_calibration(pedal_config_t *config);

// 重置为默认值
esp_err_t nvs_config_reset(void);

// 打印所有配置 (调试用)
void nvs_config_print(void);

#endif // NVS_CONFIG_H
