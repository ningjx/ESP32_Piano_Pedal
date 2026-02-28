/*
 * BLE Keyboard Implementation Header
 * TODO: Complete implementation for ESP-IDF native BLE support
 *
 * This file defines the BLE keyboard interface that needs to be implemented
 * using ESP-IDF's native BLE GAP and GATT APIs.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* BLE Keyboard API - To be implemented */

/**
 * Initialize BLE keyboard service
 * @return 0 on success, -1 on failure
 */
int ble_keyboard_init(void);

/**
 * Deinitialize BLE keyboard service
 */
void ble_keyboard_deinit(void);

/**
 * Send keyboard key press
 * @param key_code: Key code to send
 * @return 0 on success, -1 on failure
 */
int ble_keyboard_send_key(uint8_t key_code);

/**
 * Check if BLE keyboard is connected
 * @return true if connected, false otherwise
 */
bool ble_keyboard_is_connected(void);

/* Key codes (simplified subset) */
#define KEY_PAGE_UP     0xFB
#define KEY_PAGE_DOWN   0xFC

/*
 * Implementation Notes for Future Development:
 * 
 * 1. Use esp_ble_gatts_create_service() to define HID service
 * 2. Implement keyboard input report characteristics
 * 3. Set up GAP advertisement with proper flags
 * 4. Handle client connections and disconnections
 * 5. Implement the HID protocol for keyboard input
 * 
 * Required headers:
 * #include "esp_gattc_api.h"
 * #include "esp_gatts_api.h"
 * #include "esp_gap_ble_api.h"
 * #include "esp_gatt_common_api.h"
 *
 * Reference implementation structure:
 * - Create GATT server callback
 * - Define HID service UUID (0x180A)
 * - Create input report characteristic
 * - Set up advertising parameters
 * - Handle battery reporting (optional)
 */

#endif /* BLE_KEYBOARD_H */
