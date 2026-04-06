#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

// GPIO 驱动初始化
void gpio_driver_init(void);

// 输出弱音踏板开关 (true=HIGH, false=LOW)
void gpio_set_soft_switch(bool state);

// 蜂鸣器控制
void buzzer_play_tone(uint8_t degree, uint16_t duration_ms);
void buzzer_stop(void);

#endif // GPIO_DRIVER_H
