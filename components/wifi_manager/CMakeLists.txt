cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

idf_component_register(SRCS
                                        "wifi_manager.c"
                                        "wfm_miscs.c"
                    INCLUDE_DIRS .
                    REQUIRES 
                                        nvs_flash
                                        esp_event
                                        esp_wifi
                                        esp_netif
                                        lwip
                                                        )
