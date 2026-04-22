if(IDF_TARGET STREQUAL "esp32")

idf_component_register(
  SRCS "CAN.c"
  INCLUDE_DIRS "include"
  PRIV_REQUIRES
    "driver"
    "esp_driver_gpio"
    "esp_rom"
)

endif()
