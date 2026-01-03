idf_component_register(
    SRCS "device_firmware.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_now nvs_flash esp_event esp_netif json
)
