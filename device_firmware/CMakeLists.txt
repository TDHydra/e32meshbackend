cmake_minimum_required(VERSION 3.16)

project(esp32_device_firmware)

set(EXTRA_COMPONENT_DIRS ./components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project_name("esp32_device_firmware")
