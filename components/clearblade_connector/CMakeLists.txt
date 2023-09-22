idf_component_register(SRCS 

    "clearblade_connect.c"
    "sntp_time.c"
    "jwt_token_gcp.c"
    "mqtt_basico.c"
    "base64url.c"

                    INCLUDE_DIRS "."
                                        INCLUDE_DIRS .

                    REQUIRES 
                                        mqtt
                                        esp_wifi
                                        esp_netif
                                        nvs_flash
                                        esp_event
                                        esp_timer
                                        esp_http_server
                                        json
                                                        )



