idf_component_register(SRCS "main.c"
                            "display.c"
                            "ble_scan.c"
                            "rssi_filter.c"
                            "net.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES bt nvs_flash esp_driver_gpio esp_timer
                                     esp_wifi esp_event esp_netif esp_http_client)
