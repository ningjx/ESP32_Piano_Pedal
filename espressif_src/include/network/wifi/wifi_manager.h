#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"

esp_err_t wifi_init_softap(void);
esp_err_t pedal_wifi_deinit(void);

#endif
