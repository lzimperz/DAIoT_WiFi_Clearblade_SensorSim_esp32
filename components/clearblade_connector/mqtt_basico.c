/*
 * mqtt_basico.c
 *
 *  Created on: 16/3/2020
 *      Author: Leopoldo Zimperz
 *      Mail: leopoldo.a.zimperz@gmail.com
 *
 *  Basado en: ESP-IDF - MQTT (over TCP) Example
 */

/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "certs.h"
#include "mqtt_basico.h"
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"

#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "jwt_token_gcp.h"
#include "clearblade_connect.h"

static const char *TAG = "MQTT MODULE: ";

int RTC_DATA_ATTR last_error_count = 0;
int RTC_DATA_ATTR last_error_code = 0;
unsigned int RTC_DATA_ATTR last_on_time_seconds = 0;
int sntp_response_time_seconds = 0;
unsigned int tph_on_time = 0;
time_t wake_up_timestamp = 0;

esp_mqtt_client_config_t mqtt_client_config = {};
char *GCP_JWT = NULL;
bool mqtt_client_connected = false;
bool mqtt_disconnected_event_flag = false;

static bool mqtt_client_configure(void);

int T = 0, P = 0, H = 0; // las declaro global para probar rapidamente
uint8_t id_sensor_recibido;

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t esp_mqtt_client = event->client;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        // Setear bit de grupo de evengos: CONNECTED_TO_MQTT_BROKER
        xEventGroupSetBits(*mqtt_client.mqtt_event_group, CONNECTED_TO_MQTT_BROKER);
        xEventGroupClearBits(*mqtt_client.mqtt_event_group, DISCONNECTED_FROM_MQTT_BROKER);

        char bufferTopic[100];

        // Suscribirse a tema 'config' de Google Cloud IoT
        bufferTopic[0] = 0;
        strcat(bufferTopic, "/devices/");
        strcat(bufferTopic, mqtt_client.clearblade_data->deviceId);
        strcat(bufferTopic, "/config");
        esp_mqtt_client_subscribe(event->client, bufferTopic, 0);

        // Suscribirse a tema 'commands' de Google Cloud IoT
        bufferTopic[0] = 0;
        strcat(bufferTopic, "/devices/");
        strcat(bufferTopic, mqtt_client.clearblade_data->deviceId);
        strcat(bufferTopic, "/commands/#");
        esp_mqtt_client_subscribe(event->client, bufferTopic, 0);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
        xEventGroupSetBits(*mqtt_client.mqtt_event_group, DISCONNECTED_FROM_MQTT_BROKER);
        xEventGroupClearBits(*mqtt_client.mqtt_event_group, CONNECTED_TO_MQTT_BROKER);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        last_error_count = 0;
        last_error_code = 0;
        last_on_time_seconds = 0;
        sntp_response_time_seconds = 0;
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA: ");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT_EVENT_ERROR");
        last_error_count++;
        last_error_code |= ERROR_CODE_MQTT;
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%ld", base, event_id);
    mqtt_event_handler_cb(event_data);
}

void mqtt_app_main_task(void *parm)
{
    ESP_LOGI(TAG, "Ingresa a mqtt_app_main_task()");

    mqtt_client_configure();

    *mqtt_client.client_handle = esp_mqtt_client_init(&mqtt_client_config);
    esp_mqtt_client_register_event(*mqtt_client.client_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, *mqtt_client.client_handle);

    ESP_LOGI(TAG, "Arrancando MQTT client... ");
    esp_mqtt_client_start(*mqtt_client.client_handle);

    xEventGroupWaitBits(*mqtt_client.mqtt_event_group, CONNECTED_TO_MQTT_BROKER,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);
    ESP_LOGI(TAG, "Primera conexión al Broker establecida...");

    while (1)
    {
        xEventGroupWaitBits(*mqtt_client.mqtt_event_group, DISCONNECTED_FROM_MQTT_BROKER,
                            pdFALSE,
                            pdTRUE,
                            portMAX_DELAY);
        ESP_LOGW(TAG, "Reconfigurando conexión y cliente MQTT...");
        if (mqtt_client_configure())
        {
            ESP_LOGW(TAG, "Seteando configuracion Cliente MQTT... ");
            esp_mqtt_set_config(*mqtt_client.client_handle, &mqtt_client_config);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static bool mqtt_client_configure(void)
{

    ESP_LOGI(TAG, "Generando JWT Token... ");
    if (GCP_JWT != NULL)
        free(GCP_JWT);

    GCP_JWT = createGCPJWT(mqtt_client.clearblade_data->projectId, DEVICE_KEY, strlen(DEVICE_KEY), IOTCORE_TOKEN_EXPIRATION_TIME_MINUTES);

    if (GCP_JWT == 0)
    {
        last_error_count++;
        last_error_code |= ERROR_CODE_JWT;
        ESP_LOGI(TAG, "Error al generar JWT Token... ");
        return false;
    }

    mqtt_client_config.broker.address.uri = mqtt_client.clearblade_data->brokerUri;
    mqtt_client_config.credentials.username = IOTCORE_USERNAME;
    mqtt_client_config.broker.verification.certificate = CA_MIN_CERT;
    mqtt_client_config.credentials.authentication.password = GCP_JWT;
    mqtt_client_config.network.disable_auto_reconnect = false;
    mqtt_client_config.credentials.client_id = mqtt_client.clearblade_data->clientId;

    ESP_LOGI(TAG, "JWT Token generado... ");
    return true;
}
