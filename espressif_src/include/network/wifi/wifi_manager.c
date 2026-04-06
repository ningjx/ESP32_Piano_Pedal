#include "wifi_manager.h"
#include "esp_log.h"

static const char *TAG = "WIFI";

esp_err_t wifi_init_softap(void)
{
    ESP_LOGI(TAG, "WiFi SoftAP module stub");
    return ESP_OK;
}

esp_err_t pedal_wifi_deinit(void)
{
    return ESP_OK;
}
