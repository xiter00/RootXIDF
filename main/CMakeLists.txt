idf_component_register(SRCS "RootX.c"
                            "managers/boot_system.c"
                            "managers/display_system.c"
                            "managers/input_system.c"
                            "managers/wifi_system.c"
                            "managers/sd_system.c"
                            "managers/ir_system.c"
                            "managers/battery_system.c"
                            "managers/tvbgone_engine.c"
                            "managers/WORLDcodes.c"
                       INCLUDE_DIRS "include" "." "fonts"
                       REQUIRES esp_wifi nvs_flash esp_netif esp_event esp_timer driver esp_driver_gpio fatfs sdmmc vfs esp_http_server esp_driver_rmt esp_adc st7789 spiffs esp_http_client esp_https_ota app_update mbedtls esp_driver_ledc esp_driver_i2s )

target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-zmuldefs")
spiffs_create_partition_image(spiffs fonts FLASH_IN_PROJECT)
