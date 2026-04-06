set(NVS_SRCS "nvs_config.c")
add_library(nvs_config ${NVS_SRCS})
target_include_directories(nvs_config PUBLIC . PRIVATE ../../common)
target_link_libraries(nvs_config PRIVATE nvs_flash spi_flash)
