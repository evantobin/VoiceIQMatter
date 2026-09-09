set(voiceiq_sources
        "app/main.cpp"
        "faucet/touch2o_controller.cpp"
        "faucet/touch2o_protocol.cpp"
        "matter/matter_water_valve.cpp"
        "matter/project_commissionable_data_provider.cpp")

if(VOICEIQ_HTTP_DEBUG_ENABLED)
    list(APPEND voiceiq_sources "debug/web_log.cpp")
endif()

# Keep dependencies in one unconditional declaration so ESP-IDF's early
# component-requirements pass sees them. web_log.cpp and its startup path are
# still omitted entirely whenever HTTP debugging is disabled.
idf_component_register(
    SRCS ${voiceiq_sources}
    INCLUDE_DIRS "." "${CMAKE_BINARY_DIR}/generated"
    REQUIRES
        driver
        esp_event
        esp_http_server
        esp_matter
        esp_netif
        esp_timer
        espressif__mdns
        freertos
        nvs_flash)
