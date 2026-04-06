#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// OTA 更新初始化
esp_err_t ota_update_init(void);

// 开始接收固件
esp_err_t ota_update_begin(size_t image_size);

// 写入固件数据
esp_err_t ota_update_write(const void *buf, size_t size);

// 完成更新 (验证并切换分区)
esp_err_t ota_update_end(void);

// 中止更新
esp_err_t ota_update_abort(void);

// 获取更新进度 (0-100)
int ota_update_get_progress(void);

// 重启设备
void ota_device_restart(void);

// 获取当前运行分区信息 (调试用)
void ota_print_partition_info(void);

#endif // OTA_UPDATE_H
