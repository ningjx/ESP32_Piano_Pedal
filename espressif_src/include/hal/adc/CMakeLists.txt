set(ADC_SRCS "adc_driver.c")
add_library(adc_driver ${ADC_SRCS})
target_include_directories(adc_driver PUBLIC . PRIVATE ../../../common)
target_link_libraries(adc_driver PRIVATE esp_adc)
