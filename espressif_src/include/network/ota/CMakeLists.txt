set(OTA_SRCS "ota_update.c")
add_library(ota_update ${OTA_SRCS})
target_include_directories(ota_update PUBLIC . PRIVATE ../../common)
target_link_libraries(ota_update PRIVATE app_update esp_partition spi_flash)
