# Component CMakeLists.txt for Piano Pedal application
# This is the ESP-IDF style component definition

set(COMPONENT_SRCS 
    "main.c"
    "ota_portal.c"
    "ble_keyboard.c"
)

set(COMPONENT_ADD_INCLUDEDIRS 
    "../include"
)

set(COMPONENT_REQUIRES
    freertos
    esp_common
    esp_hw_support
    driver
    hal
    esp_adc_cal
    esp_rom
    esp_http_server
    esp_wifi
    nvs_flash
    esp_ota_ops
    esp_app_format
    esp_partition
    esp_system
    esp_event
    esp_log
    bootloader_support
    esp_bt
    esp_gap_ble
)

register_component()

