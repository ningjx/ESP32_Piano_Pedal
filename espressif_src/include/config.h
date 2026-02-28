/* 
 * Piano Pedal Configuration Header
 * Ported from ESP32Arduino to ESP-IDF
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ========== Firmware Version ========== */
#ifndef FW_VERSION
#define FW_VERSION "1.1.0"
#endif

/* ========== GPIO Configuration ========== */
// DAC Configuration
#define DAC_Sustain_PIN 25       // Sustain pedal voltage output
#define DAC_Sostenuto_PIN 26     // Sostenuto pedal voltage output
#define Switch_Soft_PIN 17       // Soft pedal switch output

// ADC Configuration (GPIO pins for Hall sensor input)
#define ADC_Sustain_PIN 35       // ADC1_CH7 - Sustain pedal hall sensor
#define ADC_Sostenuto_PIN 32     // ADC1_CH4 - Sostenuto pedal hall sensor
#define ADC_Soft_PIN 33          // ADC1_CH5 - Soft pedal hall sensor

// Button Configuration (LOW level active)
#define Sustain_BUTTON_PIN 27    // Sustain pedal button
#define Sostenuto_BUTTON_PIN 14  // Sostenuto pedal button
#define Soft_BUTTON_PIN 13       // Soft pedal button

// Calibration Button
#define Calibrate_Button Sostenuto_BUTTON_PIN

// Buzzer PWM Configuration
#define BUZZER_PIN 16
#define PWM_CHANNEL 0
#define PWM_FREQ 2000            // 2KHz frequency
#define PWM_RESOLUTION 8         // 8-bit resolution (0-255)

/* ========== System Configuration ========== */
#define Main_Loop_DelayMs 5
#define Max_DAC_Voltage 1.9f     // Max DAC output voltage

// Calibration timeout (milliseconds)
#define calibrationTimeoutMs 20000  // 20 seconds

// Page turner long press time
#define LongPressTimeMs 500

// ADC characteristics
#define ADC_ATTEN ADC_ATTEN_DB_11
#define ADC_WIDTH ADC_WIDTH_BIT_12
#define ADC_REF_VOLTAGE 1100     // Reference voltage in mV

/* ========== Bluetooth Configuration ========== */
#define BLUETOOTH_DEVICE_NAME "翻页器"
#define BLUETOOTH_DEVICE_MANUFACTURER "Ning"

/* ========== OTA Configuration ========== */
#define OTA_SSID "钢琴踏板固件更新"
#define OTA_AP_IP_0 192
#define OTA_AP_IP_1 168
#define OTA_AP_IP_2 4
#define OTA_AP_IP_3 1

/* ========== Debug Macros ========== */
#ifdef DEBUG
#define DBG_PRINT(...) printf(__VA_ARGS__)
#define DBG_PRINTLN(...) printf(__VA_ARGS__); printf("\n")
#define DBG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DBG_PRINT(...) ((void)0)
#define DBG_PRINTLN(...) ((void)0)
#define DBG_PRINTF(...) ((void)0)
#endif

/* ========== Type Definitions ========== */
typedef struct {
    int mv;      // Voltage in mV
    int minv;    // Minimum voltage
    int maxv;    // Maximum voltage
    int mapped;  // Mapped value 0-255
} PedalStatus_t;

typedef struct {
    int sustain_min;
    int sustain_max;
    int sostenuto_min;
    int sostenuto_max;
    int soft_min;
    int soft_max;
} CalibrationData_t;

#endif /* CONFIG_H */
