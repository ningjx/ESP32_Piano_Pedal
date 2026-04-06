#include "ota_update.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_flash_partitions.h"
#include "esp_image_format.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "OTA_UPDATE";
static esp_ota_handle_t ota_handle = 0;
static const esp_partition_t *update_partition = NULL;
static bool ota_in_progress = false;
static size_t ota_total_size = 0;
static size_t ota_written_size = 0;

/**
 * @brief OTA update initialization
 */
esp_err_t ota_update_init(void)
{
    ESP_LOGI(TAG, "OTA update initialized");
    return ESP_OK;
}

/**
 * @brief Begin OTA update - find next partition and erase
 */
esp_err_t ota_update_begin(size_t image_size)
{
    if (ota_in_progress) {
        ESP_LOGW(TAG, "OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Find next update partition
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No update partition found");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%lx",
             update_partition->subtype, update_partition->address);
    
    // Begin OTA update
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        return err;
    }
    
    ota_in_progress = true;
    ota_total_size = image_size;
    ota_written_size = 0;
    
    ESP_LOGI(TAG, "OTA update began, expected size: %zu bytes", image_size);
    return ESP_OK;
}

/**
 * @brief Write data to OTA partition
 */
esp_err_t ota_update_write(const void *buf, size_t size)
{
    if (!ota_in_progress) {
        ESP_LOGE(TAG, "OTA not in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t err = esp_ota_write(ota_handle, buf, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
        ota_update_abort();
        return err;
    }
    
    ota_written_size += size;
    
    // Log progress every 10KB
    if (ota_written_size % 10240 == 0 || ota_written_size == ota_total_size) {
        ESP_LOGI(TAG, "OTA progress: %zu / %zu bytes (%d%%)", 
                 ota_written_size, ota_total_size,
                 ota_total_size > 0 ? (int)(ota_written_size * 100 / ota_total_size) : 0);
    }
    
    return ESP_OK;
}

/**
 * @brief End OTA update and validate
 */
esp_err_t ota_update_end(void)
{
    if (!ota_in_progress) {
        ESP_LOGE(TAG, "OTA not in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        ota_update_abort();
        return err;
    }
    
    // Validate the new image
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
        return err;
    }
    
    ota_in_progress = false;
    ESP_LOGI(TAG, "OTA update completed successfully - restarting in 3 seconds");
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
    
    return ESP_OK;
}

/**
 * @brief Abort OTA update
 */
esp_err_t ota_update_abort(void)
{
    if (!ota_in_progress) {
        return ESP_OK;
    }
    
    esp_ota_abort(ota_handle);
    ota_in_progress = false;
    ota_handle = 0;
    update_partition = NULL;
    ota_written_size = 0;
    ota_total_size = 0;
    
    ESP_LOGI(TAG, "OTA update aborted");
    return ESP_OK;
}

/**
 * @brief Get OTA progress percentage
 */
int ota_update_get_progress(void)
{
    if (!ota_in_progress || ota_total_size == 0) return 0;
    return (int)(ota_written_size * 100 / ota_total_size);
}

/**
 * @brief Device restart (Stub)
 */
void ota_device_restart(void)
{
    ESP_LOGI(TAG, "OTA device restart stub");
}
