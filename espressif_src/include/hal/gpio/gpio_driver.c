#include "gpio_driver.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "GPIO_DRIVER";

// GPIO 定义
#define GPIO_SOFT_SWITCH     17   // 弱音开关输出
#define GPIO_BUZZER          16   // 蜂鸣器 PWM 输出

// 蜂鸣器频率表 (C4-B4)
static const uint16_t BUZZER_FREQS[7] = {262, 294, 330, 349, 392, 440, 494};

/**
 * @brief GPIO 驱动初始化
 */
void gpio_driver_init(void)
{
    ESP_LOGI(TAG, "Initializing GPIO driver...");

    // 初始化 GPIO (弱音开关)
    gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << GPIO_SOFT_SWITCH),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_cfg));
    gpio_set_level(GPIO_SOFT_SWITCH, 0);

    // 初始化 PWM 用于蜂鸣器
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = GPIO_BUZZER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ESP_LOGI(TAG, "GPIO driver initialized");
}

/**
 * @brief 输出弱音踏板开关
 */
void gpio_set_soft_switch(bool state)
{
    gpio_set_level(GPIO_SOFT_SWITCH, state ? 1 : 0);
}

/**
 * @brief 蜂鸣器播放音调
 */
void buzzer_play_tone(uint8_t degree, uint16_t duration_ms)
{
    if (degree < 1 || degree > 7) {
        ESP_LOGW(TAG, "Invalid tone degree: %d", degree);
        return;
    }

    uint16_t freq = BUZZER_FREQS[degree - 1];

    // 设置 PWM 频率
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);

    // 设置占空比为 50%
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    if (duration_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        buzzer_stop();
    }
}

/**
 * @brief 蜂鸣器停止
 */
void buzzer_stop(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
