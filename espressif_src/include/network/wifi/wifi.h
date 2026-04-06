#ifndef WIFI_H
#define WIFI_H

#include <stdint.h>
#include "esp_err.h"

// WiFi 配置
#define WIFI_SSID      "Piano_Pedal"
#define WIFI_PASSWORD  "12345678"
#define WIFI_CHANNEL   1
#define WIFI_MAXCONN   4

// WiFi 初始化
esp_err_t wifi_init_softap(void);

// WiFi 停止
esp_err_t wifi_stop(void);

// 检查 WiFi 连接状态
int wifi_is_connected(void);

#endif // WIFI_H
