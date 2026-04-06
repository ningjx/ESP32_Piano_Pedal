set(PEDAL_SRCS "pedal_core.c" "pedal_config.c")
add_library(pedal ${PEDAL_SRCS})
target_include_directories(pedal PUBLIC . PRIVATE ../common)
target_link_libraries(pedal PUBLIC adc_driver dac_driver PRIVATE freertos)
