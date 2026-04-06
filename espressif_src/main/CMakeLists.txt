# Modular architecture - list all source files directly
idf_component_register(
    SRCS
        # App Layer
        app_main.c
        
        # HAL Layer
        ../include/hal/adc/adc_driver.c
        ../include/hal/dac/dac_driver.c
        ../include/hal/gpio/gpio_driver.c
        
        # Pedal Core Layer
        ../include/pedal/pedal_core.c
        ../include/pedal/pedal_config.c
        
        # Storage Layer
        ../include/storage/nvs/nvs_config.c
        
        # Network Layer
        ../include/network/wifi/wifi_manager.c
        ../include/network/web/web_server.c
        ../include/network/ota/ota_update.c
        
        # Bluetooth Layer
        ../include/bluetooth/ble_keyboard.c
        ../include/bluetooth/ble_pageturner.c
        
    INCLUDE_DIRS
        .
        ../include
        ../include/hal
        ../include/hal/adc
        ../include/hal/dac
        ../include/hal/gpio
        ../include/pedal
        ../include/storage
        ../include/storage/nvs
        ../include/network
        ../include/network/wifi
        ../include/network/web
        ../include/network/ota
        ../include/bluetooth
        ../include/common
        
    PRIV_REQUIRES
        spi_flash
        esp_adc
        driver
        esp_driver_ledc
        freertos
        nvs_flash
        esp_http_server
        esp_https_ota
        app_update
        bt
)
