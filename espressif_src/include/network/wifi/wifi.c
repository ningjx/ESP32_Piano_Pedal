#include "wifi.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "WIFI";

static esp_netif_t *netif_ap = NULL;
static int connected_clients = 0;

/**
 * @brief WiFi 事件处理
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                connected_clients++;
                ESP_LOGI(TAG, "Client connected: %02X:%02X:%02X:%02X:%02X:%02X, AID=%d, Connected clients: %d",
                         event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5],
                         event->aid, connected_clients);
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                connected_clients--;
                ESP_LOGI(TAG, "Client disconnected: %02X:%02X:%02X:%02X:%02X:%02X, AID=%d, Remaining clients: %d",
                         event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5],
                         event->aid, connected_clients);
                break;
            }
            default:
                break;
        }
    }
}

/**
 * @brief 初始化 WiFi SoftAP 模式
 */
esp_err_t wifi_init_softap(void)
{
    ESP_LOGI(TAG, "Initializing WiFi SoftAP...");

    // 创建 WiFi 网络接口
    netif_ap = esp_netif_create_default_wifi_ap();
    if (netif_ap == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi AP interface");
        return ESP_FAIL;
    }

    // 初始化 WiFi 驱动程序
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件处理
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // 配置 WiFi
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASSWORD,
            .max_connection = WIFI_MAXCONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi SoftAP initialized: SSID=%s, Password=%s, IP=192.168.4.1",
             WIFI_SSID, WIFI_PASSWORD);

    return ESP_OK;
}

/**
 * @brief 停止 WiFi
 */
esp_err_t wifi_stop(void)
{
    ESP_LOGI(TAG, "Stopping WiFi...");
    esp_wifi_stop();
    esp_wifi_deinit();
    if (netif_ap) {
        esp_netif_destroy(netif_ap);
        netif_ap = NULL;
    }
    return ESP_OK;
}

/**
 * @brief 检查是否有客户端连接
 */
int wifi_is_connected(void)
{
    return connected_clients > 0 ? 1 : 0;
}
