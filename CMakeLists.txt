cmake_minimum_required(VERSION 3.5)

include(${CMAKE_CURRENT_LIST_DIR}/project_config.cmake)

string(TOLOWER "${VOICEIQ_MATTER_TRANSPORT}" VOICEIQ_MATTER_TRANSPORT)
if(VOICEIQ_MATTER_TRANSPORT STREQUAL "wifi")
    set(VOICEIQ_TRANSPORT_WIFI 1)
    set(VOICEIQ_TRANSPORT_THREAD 0)
    set(VOICEIQ_TRANSPORT_CPP "WiFi")
    set(VOICEIQ_TRANSPORT_DEFAULTS "sdkconfig.wifi.defaults")
elseif(VOICEIQ_MATTER_TRANSPORT STREQUAL "thread")
    set(VOICEIQ_TRANSPORT_WIFI 0)
    set(VOICEIQ_TRANSPORT_THREAD 1)
    set(VOICEIQ_TRANSPORT_CPP "Thread")
    set(VOICEIQ_TRANSPORT_DEFAULTS "sdkconfig.thread.defaults")
else()
    message(FATAL_ERROR "VOICEIQ_MATTER_TRANSPORT must be wifi or thread")
endif()

if(VOICEIQ_ENABLE_HTTP_DEBUG AND VOICEIQ_TRANSPORT_WIFI)
    set(VOICEIQ_HTTP_DEBUG_ENABLED 1)
else()
    set(VOICEIQ_HTTP_DEBUG_ENABLED 0)
endif()

set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/${VOICEIQ_TRANSPORT_DEFAULTS}")
# Keep ESP-IDF's generated configuration inside the selected build directory.
# Otherwise a stale repository-level sdkconfig can silently override a change
# from Wi-Fi to Thread (or the other way around).
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/main/app/project_config.h.in"
    "${CMAKE_BINARY_DIR}/generated/project_config.h"
    @ONLY)

if(NOT DEFINED ENV{ESP_MATTER_PATH})
    message(FATAL_ERROR "Set ESP_MATTER_PATH to your esp-matter checkout before configuring.")
endif()

if(NOT DEFINED ENV{ESP_MATTER_DEVICE_PATH})
    if("${IDF_TARGET}" STREQUAL "esp32c6" OR "${IDF_TARGET}" STREQUAL "")
        set(ENV{ESP_MATTER_DEVICE_PATH} "$ENV{ESP_MATTER_PATH}/device_hal/device/esp32c6_devkit_c")
    else()
        message(FATAL_ERROR "This project targets the XIAO ESP32-C6; got ${IDF_TARGET}.")
    endif()
endif()

set(ESP_MATTER_PATH $ENV{ESP_MATTER_PATH})
set(MATTER_SDK_PATH ${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include($ENV{ESP_MATTER_DEVICE_PATH}/esp_matter_device.cmake)
include($ENV{ESP_MATTER_PATH}/examples/common/cmake_common/components_include.cmake)

set(EXTRA_COMPONENT_DIRS
    "${ESP_MATTER_PATH}/examples/common"
    "${MATTER_SDK_PATH}/config/esp32/components"
    "${ESP_MATTER_PATH}/components"
    "${ESP_MATTER_PATH}/device_hal/device"
    ${extra_components_dirs_append})

project(delta_touch2o_matter)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H;-Wno-overloaded-virtual" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os" APPEND)
