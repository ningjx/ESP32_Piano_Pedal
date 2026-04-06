set(DAC_SRCS "dac_driver.c")
idf_component_register(SRCS ${DAC_SRCS}
                       PRIV_REQUIRES driver esp_driver_ledc)

