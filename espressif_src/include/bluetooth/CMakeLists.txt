set(BLE_SRCS "ble_keyboard.c" "ble_pageturner.c")
add_library(ble_keyboard ${BLE_SRCS})
target_include_directories(ble_keyboard PUBLIC . PRIVATE ../common)
target_link_libraries(ble_keyboard PRIVATE bt btdm_ble)
