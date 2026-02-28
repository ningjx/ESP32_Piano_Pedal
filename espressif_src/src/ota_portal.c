/*
 * OTA Portal Implementation
 * Over-The-Air Firmware Update Web Interface
 * Ported to ESP-IDF
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_flash_partitions.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "netif/dns.h"

#include "config.h"
#include "ota_portal.h"

static const char *TAG = "OTA_Portal";

/* ========== Global State ========== */
static bool portal_active = false;
static httpd_handle_t server_handle = NULL;

static PedalStatus_t pedal_status[3] = {
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
};

/* ========== Handle Pedal Status Update ========== */
void ota_portal_set_pedal_status(int index, int mv, int minv, int maxv, int mapped)
{
    if (index < 0 || index >= 3) return;
    pedal_status[index].mv = mv;
    pedal_status[index].minv = minv;
    pedal_status[index].maxv = maxv;
    pedal_status[index].mapped = mapped;
}

/* ========== HTTP Handlers ========== */
static esp_err_t handle_root(httpd_req_t *req)
{
    const char *html = 
        "<!doctype html>"
        "<html lang=\"zh-CN\">"
        "<head>"
        "  <meta charset=\"utf-8\">"
        "  <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "  <title>固件更新</title>"
        "  <style>"
        "    body{font-family:Segoe UI,Roboto,Arial;background:#f5f7fb;color:#222;margin:0;padding:20px}"
        "    .card{max-width:720px;margin:30px auto;padding:20px;background:#fff;border-radius:8px;box-shadow:0 6px 18px rgba(0,0,0,0.08)}"
        "    h1{font-size:20px;margin:0 0 10px}"
        "    p.note{color:#666;font-size:13px}"
        "    .row{margin:12px 0}"
        "    input[type=file]{width:100%}"
        "    .btn{display:inline-block;padding:10px 16px;border-radius:6px;background:#0078d4;color:#fff;text-decoration:none;border:none;cursor:pointer}"
        "    .btn:disabled{opacity:0.5}"
        "    .progress{width:100%;height:14px;background:#eee;border-radius:8px;overflow:hidden}"
        "    .progress > i{display:block;height:100%;width:0;background:linear-gradient(90deg,#4caf50,#8bc34a);transition:width 150ms}"
        "    .status{margin-top:8px;font-size:13px}"
        "    .small{font-size:12px;color:#888}"
        "    .vprogress{width:60px;height:140px;background:#eee;border-radius:8px;position:relative;margin:8px auto;overflow:hidden}"
        "    .vprogress>i{position:absolute;left:0;bottom:0;width:100%;height:0;background:linear-gradient(180deg,#4caf50,#8bc34a);transition:height 120ms;border-radius:0 0 8px 8px}"
        "    .pedal-row{display:flex;gap:12px;justify-content:space-between}"
        "    .pedal-label{font-weight:600;margin-bottom:6px}"
        "    .vprogress .vmax, .vprogress .vmin{position:absolute;left:50%;transform:translateX(-50%);color:#444;font-size:12px;font-weight:600}"
        "    .vprogress .vmax{top:6px}"
        "    .vprogress .vmin{bottom:6px}"
        "  </style>"
        "</head>"
        "<body>"
        "  <div class=\"card\">"
        "    <h1>延音踏板 固件在线更新</h1>"
        "    <p class=\"note small\">当前固件版本：" FW_VERSION "</p>"
        "    <p class=\"note\" style=\"color:#d32f2f;font-weight:bold;\">注意：使用在线更新功能时无法使用蓝牙翻页</p>"
        "    <p class=\"note\">在此页面上传编译生成的固件（.bin）。上传完成设备将自动重启。</p>"
        "    <div class=\"row\">"
        "      <label>选择固件文件（.bin）</label>"
        "      <input id=\"file\" type=\"file\" accept=\".bin\" />"
        "    </div>"
        "    <div class=\"row\">"
        "      <button id=\"uploadBtn\" class=\"btn\">开始上传</button>"
        "      <button id=\"cancelBtn\" class=\"btn\" style=\"background:#999;margin-left:8px;\">取消</button>"
        "    </div>"
        "    <div class=\"row\">"
        "      <div class=\"progress\"><i id=\"bar\"></i></div>"
        "      <div class=\"status\" id=\"status\">准备就绪</div>"
        "    </div>"
        "    <div class=\"row\">"
        "      <div class=\"pedal-row\">"
        "        <div style=\"flex:1;text-align:center\">"
        "          <div class=\"pedal-label\">弱音踏板</div>"
        "          <div class=\"vprogress\" id=\"v0\"><div class=\"vmax\">0</div><i></i><div class=\"vmin\">0</div></div>"
        "          <div class=\"small\" id=\"v0_txt\">0 mV</div>"
        "        </div>"
        "        <div style=\"flex:1;text-align:center\">"
        "          <div class=\"pedal-label\">持音踏板</div>"
        "          <div class=\"vprogress\" id=\"v1\"><div class=\"vmax\">0</div><i></i><div class=\"vmin\">0</div></div>"
        "          <div class=\"small\" id=\"v1_txt\">0 mV</div>"
        "        </div>"
        "        <div style=\"flex:1;text-align:center\">"
        "          <div class=\"pedal-label\">延音踏板</div>"
        "          <div class=\"vprogress\" id=\"v2\"><div class=\"vmax\">0</div><i></i><div class=\"vmin\">0</div></div>"
        "          <div class=\"small\" id=\"v2_txt\">0 mV</div>"
        "        </div>"
        "      </div>"
        "    </div>"
        "  </div>"
        "  <script>"
        "    const fileEl = document.getElementById('file');"
        "    const uploadBtn = document.getElementById('uploadBtn');"
        "    const cancelBtn = document.getElementById('cancelBtn');"
        "    const bar = document.getElementById('bar');"
        "    const status = document.getElementById('status');"
        "    let xhr = null;"
        "    function setStatus(s){ status.textContent = s; }"
        "    function setProgress(p){ bar.style.width = p + '%'; }"
        "    uploadBtn.addEventListener('click', function(){"
        "      const f = fileEl.files[0];"
        "      if(!f){ setStatus('请先选择一个 .bin 文件'); return; }"
        "      uploadBtn.disabled = true;"
        "      setStatus('开始上传...');"
        "      setProgress(0);"
        "      const fd = new FormData();"
        "      fd.append('update', f);"
        "      xhr = new XMLHttpRequest();"
        "      xhr.open('POST', '/update', true);"
        "      xhr.upload.onprogress = function(e){"
        "        if(e.lengthComputable){"
        "          const pct = Math.round(e.loaded / e.total * 100);"
        "          setProgress(pct);"
        "          setStatus('上传中：' + pct + '%');"
        "        }"
        "      };"
        "      xhr.onload = function(){"
        "        if(xhr.status===200){"
        "          setProgress(100);"
        "          setStatus('上传完成，设备将重启并应用新固件');"
        "        } else {"
        "          setStatus('上传失败：HTTP ' + xhr.status);"
        "        }"
        "        uploadBtn.disabled = false;"
        "      };"
        "      xhr.onerror = function(){ setStatus('上传发生错误'); uploadBtn.disabled = false; };"
        "      xhr.send(fd);"
        "    });"
        "    cancelBtn.addEventListener('click', function(){"
        "      if(xhr){ xhr.abort(); setStatus('已取消'); setProgress(0); uploadBtn.disabled=false; }"
        "    });"
        "    function updatePedals(){"
        "      fetch('/status').then(r=>r.json()).then(j=>{"
        "        for(let i=0;i<3;i++){"
        "          const p = j['p'+i];"
        "          if(!p) continue;"
        "          const pct = Math.round(p.mapped / 255 * 100);"
        "          const h = Math.max(0, Math.min(100, pct));"
        "          document.querySelector('#v'+i+' > i').style.height = h+'%';"
        "          document.getElementById('v'+i+'_txt').textContent = `${p.mv} mV`;"
        "        }"
        "      }).catch(e=>{});"
        "    }"
        "    setInterval(updatePedals, 100);"
        "  </script>"
        "</body>"
        "</html>";

    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

static esp_err_t handle_status(httpd_req_t *req)
{
    char json_buffer[256];
    snprintf(json_buffer, sizeof(json_buffer),
             "{\"p0\":{\"mv\":%d,\"min\":%d,\"max\":%d,\"mapped\":%d},"
             "\"p1\":{\"mv\":%d,\"min\":%d,\"max\":%d,\"mapped\":%d},"
             "\"p2\":{\"mv\":%d,\"min\":%d,\"max\":%d,\"mapped\":%d}}",
             pedal_status[0].mv, pedal_status[0].minv, pedal_status[0].maxv, pedal_status[0].mapped,
             pedal_status[1].mv, pedal_status[1].minv, pedal_status[1].maxv, pedal_status[1].mapped,
             pedal_status[2].mv, pedal_status[2].minv, pedal_status[2].maxv, pedal_status[2].mapped);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_buffer, strlen(json_buffer));
    return ESP_OK;
}

static esp_err_t handle_upload(httpd_req_t *req)
{
    char buffer[1024];
    int received;
    int remaining = req->content_len;
    
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    
    if (ota_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    esp_err_t err = esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin error: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (remaining > 0) {
        received = httpd_req_recv(req, buffer, MIN(remaining, sizeof(buffer)));
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            esp_ota_abort(ota_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, (const void *)buffer, received);
        if (err != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        remaining -= received;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA end error: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(ota_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA set boot partition error: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA update complete, restarting in 1 second...");
    httpd_resp_send(req, "OK", 2);
    
    // Restart after sending response
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
    
    return ESP_OK;
}

static esp_err_t handle_404(httpd_req_t *req)
{
    // Redirect to root for captive portal behavior
    httpd_resp_set_status(req, "302 Found");
    char location[256];
    snprintf(location, sizeof(location), "http://%s.local/", req->host);
    httpd_resp_set_hdr(req, "Location", location);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* ========== WiFi Configuration ========== */
static void configure_wifi_ap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_create_default_wifi_ap();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = OTA_SSID,
            .ssid_len = strlen(OTA_SSID),
            .password = "",
            .max_connection = 1,
            .authmode = WIFI_AUTH_OPEN,
            .channel = 1,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Try to set fixed IP
    esp_netif_t *netif = esp_netif_next(NULL);
    if (netif != NULL) {
        esp_netif_ip_info_t ip_info;
        IP4_ADDR(&ip_info.ip, OTA_AP_IP_0, OTA_AP_IP_1, OTA_AP_IP_2, OTA_AP_IP_3);
        IP4_ADDR(&ip_info.gw, OTA_AP_IP_0, OTA_AP_IP_1, OTA_AP_IP_2, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        esp_netif_set_ip_info(netif, &ip_info);
    }

    ESP_LOGI(TAG, "WiFi AP initialized, IP: " OTA_AP_IP_0 "." OTA_AP_IP_1 "." OTA_AP_IP_2 "." OTA_AP_IP_3);
}

/* ========== HTTP Server Start ========== */
static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 5;
    
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get_root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = handle_root,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get_root);
        
        httpd_uri_t uri_get_status = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = handle_status,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get_status);
        
        httpd_uri_t uri_post_update = {
            .uri = "/update",
            .method = HTTP_POST,
            .handler = handle_upload,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_post_update);
        
        // Register 404 handler
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, handle_404);
        
        return server;
    }
    
    return NULL;
}

/* ========== OTA Portal API ========== */
void ota_portal_begin(void)
{
    if (portal_active) {
        return;
    }
    
    portal_active = true;
    
    ESP_LOGI(TAG, "Starting OTA Portal...");
    
    // Configure WiFi AP
    configure_wifi_ap();
    
    // Start web server
    server_handle = start_webserver();
    if (server_handle == NULL) {
        ESP_LOGE(TAG, "Failed to start webserver");
        portal_active = false;
        return;
    }
    
    ESP_LOGI(TAG, "OTA Portal started successfully");
    ESP_LOGI(TAG, "Connect to WiFi SSID: %s", OTA_SSID);
    ESP_LOGI(TAG, "Open browser to: http://192.168.4.1");
}

void ota_portal_handle(void)
{
    // In ESP-IDF, the HTTP server is event-driven
    // No special handling needed in the main loop
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

void ota_portal_stop(void)
{
    if (!portal_active) {
        return;
    }
    
    if (server_handle != NULL) {
        httpd_stop(server_handle);
        server_handle = NULL;
    }
    
    esp_wifi_stop();
    ESP_LOGI(TAG, "OTA Portal stopped");
    portal_active = false;
}

bool ota_portal_active(void)
{
    return portal_active;
}
