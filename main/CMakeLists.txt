idf_component_register(
    SRCS
        "app/main.cpp"
        "debug/web_log.cpp"
        "faucet/touch2o_controller.cpp"
        "matter/matter_water_valve.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_event
        esp_http_server
        esp_matter
        esp_netif
        esp_timer
        freertos
        nvs_flash
)
