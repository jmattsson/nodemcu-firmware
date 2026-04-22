idf_component_register(
  SRCS
#    "adc.c" FIXME needs driver migration
#    "dht.c" FIXME needs driver migration
#    "onewire.c" FIXME needs driver migration
    "platform.c"
    "platform_flash.c"
    "platform_partition.c"
#    "platform_rmt.c" FIXME needs driver migration
#    "sigmadelta.c"FIXME needs driver migration
#    "u8x8_nodemcu_hal.c" FIXME needs driver migration
    "ucg_nodemcu_hal.c"
    "wdt.c"
#    "ws2812.c" FIXME needs driver migration
  INCLUDE_DIRS "include"
  REQUIRES
    "driver"
    "driver_i2c"
    "esp_driver_gpio"
    "esp_driver_spi"
    "esp_driver_uart"
    "spi_flash"
    "spiffs"
    "task"
    "u8g2"
    "ucg"
  PRIV_REQUIRES
    "bootloader_support"
    "esp_hal_gpio"
    "esp_rom"
    "lua"
    "soc"
)
