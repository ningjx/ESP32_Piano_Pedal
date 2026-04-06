#include "nvs_config.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "NVS_CONFIG";
static nvs_handle_t nvs_partition_handle = 0;

/**
 * @brief NVS 初始化
 */
esp_err_t nvs_config_init(void)
{
    ESP_LOGI(TAG, "Initializing NVS...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 分区满或版本不匹配，擦除并重新初始化
        ESP_LOGI(TAG, "Erasing NVS partition...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized successfully");
    return ESP_OK;
}

/**
 * @brief NVS 反初始化
 */
void nvs_config_deinit(void)
{
    nvs_flash_deinit();
    ESP_LOGI(TAG, "NVS deinitialized");
}

/**
 * @brief 加载配置
 */
esp_err_t nvs_config_load(pedal_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t ret;
    nvs_handle_t nvs_handle;

    // 打开 NVS 命名空间 (只读模式)
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    // 读取所有参数
    nvs_get_u16(nvs_handle, NVS_KEY_SUSTAIN_MIN, &config->sustain_min_mv);
    nvs_get_u16(nvs_handle, NVS_KEY_SUSTAIN_MAX, &config->sustain_max_mv);
    nvs_get_u16(nvs_handle, NVS_KEY_SOSTENUTO_MIN, &config->sostenuto_min_mv);
    nvs_get_u16(nvs_handle, NVS_KEY_SOSTENUTO_MAX, &config->sostenuto_max_mv);
    nvs_get_u16(nvs_handle, NVS_KEY_SOFT_MIN, &config->soft_min_mv);
    nvs_get_u16(nvs_handle, NVS_KEY_SOFT_MAX, &config->soft_max_mv);
    nvs_get_u16(nvs_handle, NVS_KEY_HALF_LOWER, &config->half_pedal_lower_mv);
    nvs_get_u16(nvs_handle, NVS_KEY_HALF_UPPER, &config->half_pedal_upper_mv);
    nvs_get_i32(nvs_handle, NVS_KEY_HALF_VOLTAGE, (int32_t *)&config->half_pedal_voltage);
    
    uint8_t bt_active = 0;
    nvs_get_u8(nvs_handle, NVS_KEY_BT_ACTIVE, &bt_active);
    config->bluetooth_active = (bool)bt_active;

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Configuration loaded from NVS");
    return ESP_OK;
}

/**
 * @brief 保存配置
 */
esp_err_t nvs_config_save(const pedal_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t ret;
    nvs_handle_t nvs_handle;

    // 打开 NVS 命名空间 (读写模式)
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    // 保存所有参数
    nvs_set_u16(nvs_handle, NVS_KEY_SUSTAIN_MIN, config->sustain_min_mv);
    nvs_set_u16(nvs_handle, NVS_KEY_SUSTAIN_MAX, config->sustain_max_mv);
    nvs_set_u16(nvs_handle, NVS_KEY_SOSTENUTO_MIN, config->sostenuto_min_mv);
    nvs_set_u16(nvs_handle, NVS_KEY_SOSTENUTO_MAX, config->sostenuto_max_mv);
    nvs_set_u16(nvs_handle, NVS_KEY_SOFT_MIN, config->soft_min_mv);
    nvs_set_u16(nvs_handle, NVS_KEY_SOFT_MAX, config->soft_max_mv);
    nvs_set_u16(nvs_handle, NVS_KEY_HALF_LOWER, config->half_pedal_lower_mv);
    nvs_set_u16(nvs_handle, NVS_KEY_HALF_UPPER, config->half_pedal_upper_mv);
    nvs_set_i32(nvs_handle, NVS_KEY_HALF_VOLTAGE, *(int32_t *)&config->half_pedal_voltage);
    nvs_set_u8(nvs_handle, NVS_KEY_BT_ACTIVE, config->bluetooth_active ? 1 : 0);

    // 提交更改
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save configuration: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Configuration saved to NVS");
    }

    return ret;
}

/**
 * @brief 保存校准数据
 */
esp_err_t nvs_save_calibration(const pedal_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t ret;
    nvs_handle_t nvs_handle;

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    // 仅保存校准范围
    nvs_set_u16(nvs_handle, NVS_KEY_SUSTAIN_MIN, config->sustain_min_mv);
    nvs_set_u16(nvs_handle, NVS_KEY_SUSTAIN_MAX, config->sustain_max_mv);
    nvs_set_u16(nvs_handle, NVS_KEY_SOSTENUTO_MIN, config->sostenuto_min_mv);
    nvs_set_u16(nvs_handle, NVS_KEY_SOSTENUTO_MAX, config->sostenuto_max_mv);
    nvs_set_u16(nvs_handle, NVS_KEY_SOFT_MIN, config->soft_min_mv);
    nvs_set_u16(nvs_handle, NVS_KEY_SOFT_MAX, config->soft_max_mv);

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Calibration data saved");
    return ret;
}

/**
 * @brief 加载校准数据
 */
esp_err_t nvs_load_calibration(pedal_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t ret;
    nvs_handle_t nvs_handle;

    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) return ret;

    // 读取校准范围
    nvs_get_u16(nvs_handle, NVS_KEY_SUSTAIN_MIN, &config->sustain_min_mv);
    nvs_get_u16(nvs_handle, NVS_KEY_SUSTAIN_MAX, &config->sustain_max_mv);
    nvs_get_u16(nvs_handle, NVS_KEY_SOSTENUTO_MIN, &config->sostenuto_min_mv);
    nvs_get_u16(nvs_handle, NVS_KEY_SOSTENUTO_MAX, &config->sostenuto_max_mv);
    nvs_get_u16(nvs_handle, NVS_KEY_SOFT_MIN, &config->soft_min_mv);
    nvs_get_u16(nvs_handle, NVS_KEY_SOFT_MAX, &config->soft_max_mv);

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Calibration data loaded");
    return ESP_OK;
}

/**
 * @brief 重置为默认值
 */
esp_err_t nvs_config_reset(void)
{
    esp_err_t ret;
    nvs_handle_t nvs_handle;

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    nvs_erase_all(nvs_handle);
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "NVS configuration reset to defaults");
    return ret;
}

/**
 * @brief 打印NVS配置 (调试用)
 */
void nvs_config_print(void)
{
    esp_err_t ret;
    nvs_handle_t nvs_handle;

    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Cannot open NVS namespace for printing");
        return;
    }

    uint16_t val16;
    uint8_t val8;
    int32_t val32;
    float valf;

    ESP_LOGI(TAG, "=== NVS Configuration ===");

    if (nvs_get_u16(nvs_handle, NVS_KEY_SUSTAIN_MIN, &val16) == ESP_OK)
        ESP_LOGI(TAG, "Sustain Min: %d mV", val16);

    if (nvs_get_u16(nvs_handle, NVS_KEY_SUSTAIN_MAX, &val16) == ESP_OK)
        ESP_LOGI(TAG, "Sustain Max: %d mV", val16);

    if (nvs_get_u16(nvs_handle, NVS_KEY_HALF_LOWER, &val16) == ESP_OK)
        ESP_LOGI(TAG, "Half-Pedal Lower: %d mV", val16);

    if (nvs_get_u16(nvs_handle, NVS_KEY_HALF_UPPER, &val16) == ESP_OK)
        ESP_LOGI(TAG, "Half-Pedal Upper: %d mV", val16);

    if (nvs_get_i32(nvs_handle, NVS_KEY_HALF_VOLTAGE, &val32) == ESP_OK) {
        valf = *(float *)&val32;
        ESP_LOGI(TAG, "Half-Pedal Voltage: %.1f V", valf);
    }

    if (nvs_get_u8(nvs_handle, NVS_KEY_BT_ACTIVE, &val8) == ESP_OK)
        ESP_LOGI(TAG, "Bluetooth Active: %s", val8 ? "Yes" : "No");

    ESP_LOGI(TAG, "========================");

    nvs_close(nvs_handle);
}
