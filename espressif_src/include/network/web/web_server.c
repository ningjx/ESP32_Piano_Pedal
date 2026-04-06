#include "web_server.h"
#include "web_assets.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "pedal_core.h"
#include "pedal_config.h"
#include "nvs_config.h"
#include "adc_driver.h"
#include "ota_update.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;

// Forward declarations for pedal status
typedef struct {
    uint16_t mv;
    uint16_t min;
    uint16_t max;
    uint16_t mapped;
} pedal_status_t;

/**
 * @brief Get current pedal status (linked to pedal_core.c)
 */
static void get_pedal_status(uint8_t pedal_id, pedal_status_t *status) {
    if (!status) return;
    
    // Get real-time pedal state
    pedal_state_t state = pedal_get_state();
    pedal_config_t *config = pedal_config_get();
    
    switch (pedal_id) {
        case 0:  // Soft pedal (p0)
            status->mv = state.soft_mv;
            status->min = config->soft_min_mv;
            status->max = config->soft_max_mv;
            break;
        case 1:  // Sostenuto pedal (p1)
            status->mv = state.sostenuto_mv;
            status->min = config->sostenuto_min_mv;
            status->max = config->sostenuto_max_mv;
            break;
        case 2:  // Sustain pedal (p2)
            status->mv = state.sustain_mv;
            status->min = config->sustain_min_mv;
            status->max = config->sustain_max_mv;
            break;
        default:
            status->mv = 0;
            status->min = 0;
            status->max = 3300;
            break;
    }
    
    // Calculate mapped value (0-255)
    uint16_t range = status->max - status->min;
    if (range > 0 && status->mv >= status->min && status->mv <= status->max) {
        status->mapped = (status->mv - status->min) * 255 / range;
    } else {
        status->mapped = 0;
    }
}

/**
 * @brief GET / - Main HTML page
 */
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, WEB_PAGE_HTML, strlen(WEB_PAGE_HTML));
    return ESP_OK;
}

/**
 * @brief GET /status - Pedal status JSON
 */
static esp_err_t status_handler(httpd_req_t *req) {
    pedal_status_t pedals[3];
    for (int i = 0; i < 3; i++) get_pedal_status(i, &pedals[i]);
    
    // Get half-pedal configuration
    pedal_config_t *config = pedal_config_get();
    
    // Manual JSON construction with real data
    char json_response[512];
    snprintf(json_response, sizeof(json_response),
        "{"
        "\"p0\":{\"mv\":%d,\"min\":%d,\"max\":%d,\"mapped\":%d},"
        "\"p1\":{\"mv\":%d,\"min\":%d,\"max\":%d,\"mapped\":%d},"
        "\"p2\":{\"mv\":%d,\"min\":%d,\"max\":%d,\"mapped\":%d},"
        "\"halfPedal\":{\"lower\":%d,\"upper\":%d,\"voltage\":%.1f}"
        "}",
        pedals[0].mv, pedals[0].min, pedals[0].max, pedals[0].mapped,
        pedals[1].mv, pedals[1].min, pedals[1].max, pedals[1].mapped,
        pedals[2].mv, pedals[2].min, pedals[2].max, pedals[2].mapped,
        config->half_pedal_lower_mv,
        config->half_pedal_upper_mv,
        config->half_pedal_voltage
    );
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, strlen(json_response));
    return ESP_OK;
}

/**
 * @brief GET /setHalfPedal - Set half-pedal range
 */
static esp_err_t set_half_pedal_handler(httpd_req_t *req) {
    char buf[256] = {0};
    httpd_req_get_url_query_str(req, buf, sizeof(buf) - 1);
    
    char param[32];
    int lower = -1, upper = -1;
    float voltage = -1.0;
    
    if (httpd_query_key_value(buf, "lower", param, sizeof(param)) == ESP_OK) {
        lower = atoi(param);
        ESP_LOGI(TAG, "Setting lower half-pedal: %d mV", lower);
    }
    if (httpd_query_key_value(buf, "upper", param, sizeof(param)) == ESP_OK) {
        upper = atoi(param);
        ESP_LOGI(TAG, "Setting upper half-pedal: %d mV", upper);
    }
    if (httpd_query_key_value(buf, "voltage", param, sizeof(param)) == ESP_OK) {
        voltage = atof(param);
        ESP_LOGI(TAG, "Setting voltage: %.1f V", voltage);
    }
    
    // Save to NVS if parameters changed
    pedal_config_t *config = pedal_config_get();
    bool changed = false;
    
    if (lower >= 0 && lower <= 3300) {
        config->half_pedal_lower_mv = lower;
        changed = true;
    }
    if (upper >= 0 && upper <= 3300) {
        config->half_pedal_upper_mv = upper;
        changed = true;
    }
    if (voltage >= 0.0f && voltage <= 3.3f) {
        config->half_pedal_voltage = voltage;
        changed = true;
    }
    
    if (changed) {
        esp_err_t ret = nvs_config_save(config);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Half-pedal settings saved to NVS");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":true}", 16);
        } else {
            ESP_LOGE(TAG, "Failed to save half-pedal settings");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":false,\"error\":\"invalid parameters\"}", 42);
    }
    
    return ESP_OK;
}

/**
 * @brief POST /update - OTA firmware upload
 */
static esp_err_t update_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Firmware update request received, content length: %d", req->content_len);
    
    if (req->content_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No content");
        return ESP_FAIL;
    }
    
    // Begin OTA update
    esp_err_t err = ota_update_begin(req->content_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to begin OTA update");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Receive and write firmware data
    char *buf = malloc(4096);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        ota_update_abort();
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    int remaining = req->content_len;
    int received = 0;
    
    while (remaining > 0) {
        int to_recv = (remaining > 4096) ? 4096 : remaining;
        int ret = httpd_req_recv(req, buf, to_recv);
        
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;  // Retry on timeout
            }
            ESP_LOGE(TAG, "Failed to receive data");
            free(buf);
            ota_update_abort();
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        // Write to OTA partition
        err = ota_update_write(buf, ret);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write OTA data");
            free(buf);
            ota_update_abort();
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        remaining -= ret;
        received += ret;
    }
    
    free(buf);
    
    // Finish OTA update
    err = ota_update_end();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to finish OTA update");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Firmware update completed successfully");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true,\"message\":\"Update complete, restarting...\"}", 56);
    
    return ESP_OK;
}

/**
 * @brief POST /calibrate - Calibrate pedals
 */
static esp_err_t calibrate_handler(httpd_req_t *req) {
    // TODO: Implement calibration
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"calibrating\"}", 24);
    return ESP_OK;
}

/**
 * @brief POST /bluetooth - Toggle BLE
 */
static esp_err_t bluetooth_handler(httpd_req_t *req) {
    // TODO: Implement BLE control
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", 16);
    return ESP_OK;
}

/**
 * @brief Start web server
 */
esp_err_t web_server_start(void) {
    ESP_LOGI(TAG, "Starting web server...");
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server");
        return ESP_FAIL;
    }
    
    httpd_uri_t uris[] = {
        {.uri="/", .method=HTTP_GET, .handler=root_handler},
        {.uri="/status", .method=HTTP_GET, .handler=status_handler},
        {.uri="/setHalfPedal", .method=HTTP_GET, .handler=set_half_pedal_handler},
        {.uri="/update", .method=HTTP_POST, .handler=update_handler},
        {.uri="/calibrate", .method=HTTP_POST, .handler=calibrate_handler},
        {.uri="/bluetooth", .method=HTTP_POST, .handler=bluetooth_handler}
    };
    
    for (int i = 0; i < 6; i++) httpd_register_uri_handler(server, &uris[i]);
    ESP_LOGI(TAG, "Web server started on 192.168.4.1");
    return ESP_OK;
}

/**
 * @brief Stop web server
 */
esp_err_t web_server_stop(void) {
    return (server) ? httpd_stop(server) : ESP_OK;
}
