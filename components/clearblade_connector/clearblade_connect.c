/*
 * clearblade_coneect.c
 *
 *  Created on: 14/9/2023
 *      Author: Leopoldo Zimperz
 *      Mail: leopoldo.a.zimperz@gmail.com
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "sntp_time.h"
#include "clearblade_connect.h"
#include "mqtt_basico.h"
#include "string.h"

#define CLEARBLADE_DEFAULT_BROKER_URI "mqtts://us-central1-mqtt.clearblade.com"

extern bool time_sinc_ok;

clearblade_data_t clearblade_data;
esp_mqtt_client_handle_t client_handle = NULL;

/* FreeRTOS event group - Clearblade client state */
EventGroupHandle_t mqtt_client_event_group;

void set_clearblade_data(const char *brokerUri, const char *projectId, const char *region, const char *registry, const char *deviceId)
{
    if (brokerUri != NULL)
    {
        clearblade_data.brokerUri = malloc(strlen(brokerUri) + 1);
        strcpy(clearblade_data.brokerUri, brokerUri);
    }
    else
    {
        clearblade_data.brokerUri = malloc(strlen(CLEARBLADE_DEFAULT_BROKER_URI) + 1);
        strcpy(clearblade_data.brokerUri, CLEARBLADE_DEFAULT_BROKER_URI);
    }

    clearblade_data.projectId = malloc(sizeof(projectId) + 100);
    strcpy(clearblade_data.projectId, projectId);

    clearblade_data.region = malloc(sizeof(region) + 100);
    strcpy(clearblade_data.region, region);

    clearblade_data.registry = malloc(sizeof(registry) + 100);
    strcpy(clearblade_data.registry, registry);

    clearblade_data.deviceId = malloc(sizeof(deviceId) + 100);
    strcpy(clearblade_data.deviceId, deviceId);

    clearblade_data.clientId = malloc(strlen(projectId) + strlen(region) + strlen(registry) + strlen(deviceId) + 500);
    sprintf(clearblade_data.clientId,
            "projects/%s/locations/%s/registries/%s/devices/%s",
            clearblade_data.projectId,
            clearblade_data.region,
            clearblade_data.registry,
            clearblade_data.deviceId);
};

void start(void)
{

    mqtt_client_event_group = xEventGroupCreate();

    xEventGroupWaitBits(mqtt_client_event_group, NETWORK_AVAILABLE,
                        pdFALSE,
                        pdFALSE,
                        portMAX_DELAY);

    initialize_sntp();

    xEventGroupWaitBits(mqtt_client_event_group, NETWORK_AVAILABLE | TIME_SYNCHRONIZED,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);

    xTaskCreate(mqtt_app_main_task, "mqtt_app_task", 4096 * 10, NULL, 3, NULL);
}

void set_network_available_flag(bool is_network_available)
{
    if (is_network_available)
        xEventGroupSetBits(mqtt_client_event_group, NETWORK_AVAILABLE);
    else
        xEventGroupClearBits(mqtt_client_event_group, NETWORK_AVAILABLE);
}

/*****************************************************
 *   Driver Instance Declaration(s) API(s)            *
 ******************************************************/
const mqtt_client_t mqtt_client = {
    // Clearblade Connector Props
    .mqtt_event_group = &mqtt_client_event_group,
    .client_handle = &client_handle,
    .clearblade_data = &clearblade_data,
    // Clearblade Connector Functions
    .set_clearblade_data = set_clearblade_data,
    .start = start,
    .set_network_available_flag = set_network_available_flag,
};