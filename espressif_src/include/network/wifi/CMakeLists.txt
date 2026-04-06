set(WIFI_SRCS "wifi_manager.c")
add_library(wifi_manager ${WIFI_SRCS})
target_include_directories(wifi_manager PUBLIC . PRIVATE ../../common)
target_link_libraries(wifi_manager PRIVATE esp_wifi esp_event esp_netif)
