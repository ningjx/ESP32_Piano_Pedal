#ifndef PEDAL_CORE_H
#define PEDAL_CORE_H

#include <stdint.h>
#include <stdbool.h>

// 踏板状态结构体
typedef struct {
    uint8_t sustain_value;
    uint8_t sostenuto_value;
    uint8_t soft_value;
    uint16_t sustain_mv;
    uint16_t sostenuto_mv;
    uint16_t soft_mv;
} pedal_state_t;

// 踏板配置结构体
typedef struct {
    uint16_t sustain_min_mv;
    uint16_t sustain_max_mv;
    uint16_t sostenuto_min_mv;
    uint16_t sostenuto_max_mv;
    uint16_t soft_min_mv;
    uint16_t soft_max_mv;
    uint16_t half_pedal_lower_mv;
    uint16_t half_pedal_upper_mv;
    float half_pedal_voltage;
    bool bluetooth_active;
} pedal_config_t;

// 初始化
void pedal_init(void);

// 获取状态
pedal_state_t pedal_get_state(void);

// 启动/停止处理
void pedal_start_processing(void);
void pedal_stop_processing(void);

// 校准模式
void pedal_start_calibration(void);
bool pedal_is_calibrating(void);
void pedal_finish_calibration(void);
void pedal_cancel_calibration(void);

#endif // PEDAL_CORE_H
