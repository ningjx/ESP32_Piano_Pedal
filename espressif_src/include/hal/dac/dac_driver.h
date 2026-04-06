#ifndef DAC_DRIVER_H
#define DAC_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

// DAC 输出值范围假设 0-1.9V
#define DAC_MAX_VOLTAGE 1.9f

// GPIO 定义
#define DAC_SUSTAIN_GPIO     25   // DAC1
#define DAC_SOSTENUTO_GPIO   26   // DAC2
#define GPIO_SOFT_SWITCH     17   // 弱音开关输出
#define GPIO_BUZZER          16   // 蜂鸣器 PWM 输出

// DAC 驱动初始化
void dac_driver_init(void);

// 输出延音踏板 DAC 值 (0-255)
void dac_output_sustain(uint8_t value);

// 输出持音踏板 DAC 值 (0-255)
void dac_output_sostenuto(uint8_t value);

// 输出弱音踏板开关 (true=HIGH, false=LOW)
void dac_output_soft_switch(bool state);

// 蜂鸣器控制: 播放指定频率 (Hz) 和时长 (ms)
// degree: 1-7 对应 C4-B4 (262-494 Hz)
void buzzer_play_tone(uint8_t degree, uint16_t duration_ms);

// 蜂鸣器停止
void buzzer_stop(void);

#endif // DAC_DRIVER_H
