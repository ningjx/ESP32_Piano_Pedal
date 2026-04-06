#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"

// Web 服务器启动
esp_err_t web_server_start(void);

// Web 服务器停止
esp_err_t web_server_stop(void);

// 检查服务器状态
int web_server_is_running(void);

#endif // WEB_SERVER_H
