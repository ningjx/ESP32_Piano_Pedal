#include "dac_driver.h"
#include "gpio_driver.h"
#include "driver/dac_oneshot.h"
#include "esp_log.h"

static const char *TAG = "DAC_DRIVER";

// DAC handles (DAC1 and DAC2 for GPIO25 and GPIO26)
static dac_oneshot_handle_t dac_handle_sustain = NULL;
static dac_oneshot_handle_t dac_handle_sostenuto = NULL;

/**
 * @brief DAC driver initialization
 */
void dac_driver_init(void)
{
    ESP_LOGI(TAG, "Initializing DAC driver...");

    // Initialize DAC1 (GPIO25) - sustain pedal
    dac_oneshot_config_t config1 = {
        .chan_id = DAC_CHAN_0,
    };
    ESP_ERROR_CHECK(dac_oneshot_new_channel(&config1, &dac_handle_sustain));

    // Initialize DAC2 (GPIO26) - sostenuto pedal
    dac_oneshot_config_t config2 = {
        .chan_id = DAC_CHAN_1,
    };
    ESP_ERROR_CHECK(dac_oneshot_new_channel(&config2, &dac_handle_sostenuto));

    ESP_LOGI(TAG, "DAC driver initialized");
}

/**
 * @brief Convert 0-255 value to DAC output (0-255)
 */
static inline uint8_t value_to_dac_output(uint8_t value)
{
    return value;
}

/**
 * @brief Output sustain pedal DAC value
 */
void dac_output_sustain(uint8_t value)
{
    uint8_t dac_value = value_to_dac_output(value);
    dac_oneshot_output_voltage(dac_handle_sustain, dac_value);
}

/**
 * @brief Output sostenuto pedal DAC value
 */
void dac_output_sostenuto(uint8_t value)
{
    uint8_t dac_value = value_to_dac_output(value);
    dac_oneshot_output_voltage(dac_handle_sostenuto, dac_value);
}

/**
 * @brief Output soft pedal switch (via GPIO)
 */
void dac_output_soft_switch(bool state)
{
    // This is now handled by gpio_set_soft_switch in gpio_driver.c
    gpio_set_soft_switch(state);
}
