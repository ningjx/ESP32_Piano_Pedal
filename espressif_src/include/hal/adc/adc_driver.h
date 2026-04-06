#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include <stdint.h>

// 踏板类型定义
typedef enum {
    PEDAL_SUSTAIN = 0,      // 延音踏板 (GPIO35)
    PEDAL_SOSTENUTO = 1,    // 持音踏板 (GPIO32)
    PEDAL_SOFT = 2          // 弱音踏板 (GPIO33)
} pedal_type_t;

// ADC 驱动初始化
void adc_driver_init(void);

// 读取踏板模拟值 (0-255)
uint8_t adc_read_pedal_value(pedal_type_t pedal_id);

// 读取原始 ADC 毫伏值
uint16_t adc_read_pedal_mv(pedal_type_t pedal_id);

// 设置 ADC 校准范围
void adc_set_calibration(pedal_type_t pedal_id, uint16_t min_mv, uint16_t max_mv);

// 获取 ADC 校准范围
void adc_get_calibration(pedal_type_t pedal_id, uint16_t *min_mv, uint16_t *max_mv);

#endif // ADC_DRIVER_H
