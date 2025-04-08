/*
 * main.c
 *
 *  Created on: 14/9/2023
 *      Author: Leopoldo Zimperz
 *      Mail: leopoldo.a.zimperz@gmail.com
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi_manager.h"
#include "temp_sensor.h"
#include "clearblade_connect.h"

#define WIFI_SSID "wifi-ssid"     // !!!!!!!!!!! Configurar
#define WIFI_PASSWORD "wifi-pass" // !!!!!!!!!!! Configurar
#define CLEARBLADE_BROKER_URI "mqtts://us-central1-mqtt.clearblade.com"
#define CLEARBLADE_PROJECT_ID "daiot-practica"
#define CLEARBLADE_REGION "us-central1"
#define CLEARBLADE_REGISTRY "registry_1"

// Configurar CLEARBLADE_DEVICE_ID segun tu nombre
#define CLEARBLADE_DEVICE_ID "device-10x" // Ejemplo para Leopoldo: "device-101"
// 101	Leopoldo
// 102	Pablo Horacio
// 103	Eberth Gabriel
// 104	Jose Pedro
// 105	Juan David
// 106	Martin Anibal
// 107	Rodrigo Jurgen
// 108	Mariángel José
// 109	Luis Gabriel
// 110	María Antonella
// 111	Jesús Enrique
// 112	Denis Jorge
// 113	Ernesto Enrique

static const char *TAG = "Main section";

void wifi_got_ip_event_callback(void)
{
    mqtt_client.set_network_available_flag(true);
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Default Event Loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Wi-Fi manager configuration
    wifi_manager.wifi_init();
    wifi_manager.set_sta_credentials(WIFI_SSID, WIFI_PASSWORD);
    wifi_manager.set_got_ip_callback(wifi_got_ip_event_callback);

    // Clearblade MQTT cliente configuration
    mqtt_client.set_clearblade_data(
        CLEARBLADE_BROKER_URI,
        CLEARBLADE_PROJECT_ID,
        CLEARBLADE_REGION,
        CLEARBLADE_REGISTRY,
        CLEARBLADE_DEVICE_ID);
    mqtt_client.start();

    // Temp sensor simulator config
    tempSensor.initialize();
    tempSensor.set_mqtt_info("", CLEARBLADE_DEVICE_ID, mqtt_client.client_handle);

    /* Main loop */
    while (true)
    {
        tempSensor.sample_temp();
        ESP_LOGI(TAG, "Temp: %s", tempSensor.temp_string);
        xEventGroupWaitBits(*mqtt_client.mqtt_event_group, NETWORK_AVAILABLE | CONNECTED_TO_MQTT_BROKER,
                            pdFALSE,
                            pdTRUE,
                            portMAX_DELAY);
        tempSensor.publish_to_mqtt();
        vTaskDelay(4 * 60 * 1000 / portTICK_PERIOD_MS); // publica cada 4 minutos
    }
}
