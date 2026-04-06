/**
 * @file hal.h
 * @brief Hardware Abstraction Layer - Unified Interface
 */
#pragma once

#include "adc/adc_driver.h"
#include "dac/dac_driver.h"
#include "gpio/gpio_driver.h"

/**
 * @brief Initialize all hardware drivers
 */
void hal_init(void);
