cmake_minimum_required(VERSION 3.16)

project(esp32c6_motion_sensor)

# Add all source files
set(SOURCES
    main.c
    device_config.c
    display_driver.c
    display_ui.c
    motion_sensor.c
    http_server.c
    esp_now_device.c
)

# Add component
idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES driver esp_http_server esp_wifi esp_now cjson
)

# Display driver optimizations
target_compile_options(${COMPONENT_LIB} PRIVATE -O2)
