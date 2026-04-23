idf_component_register(
  SRCS "ip_fmt.c" "user_main.c"
  INCLUDE_DIRS "include"
  REQUIRES "lua"
  PRIV_REQUIRES
    "driver"
    "esp_netif"
    "esp_stdio"
    "esp_usb_cdc_rom_console"
    "nvs_flash"
    "spiffs"
    "vfs"
  LDFRAGMENTS "nodemcu.lf"
)
