#ifndef BLE_KEYBOARD_H
#define BLE_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// BLE Keyboard 初始化
esp_err_t ble_keyboard_init(void);

// BLE Keyboard 停止
esp_err_t ble_keyboard_deinit(void);

// 发送 Page Down 键
void ble_keyboard_send_pagedown(void);

// 发送 Page Up 键
void ble_keyboard_send_pageup(void);

// 检查 BLE 连接状态
bool ble_keyboard_is_connected(void);

// 获取当前连接设备数量
uint8_t ble_keyboard_get_connection_count(void);

#endif // BLE_KEYBOARD_H
