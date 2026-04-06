set(GPIO_SRCS "gpio_driver.c")
add_library(gpio_driver ${GPIO_SRCS})
target_include_directories(gpio_driver PUBLIC . PRIVATE ../../../common)
target_link_libraries(gpio_driver PRIVATE driver)
