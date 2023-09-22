/*
 * temp_sensor.c
 *
 *  Created on: 14/9/2023
 *      Author: Leopoldo Zimperz
 *      Mail: leopoldo.a.zimperz@gmail.com
 *
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include <string.h>

#include "temp_sensor.h"

#define SENSOR_LOG_TAG "SENSOR_SIM"

/************************************************************************/
/* Simula sensor de temperatura                                         */
/*                                                                      */
/* La temperatura, en vez de ser un número random, lo que hace          */
/* sumar un delta hacia arriba o hacia abajo, según un random           */
/* Por el motivo anterior, necesito que la variable "temp" perdure      */
/* tras los reinicios por software y esto se logra guardandola en la    */
/* memoria del reloj de tiempo real RTC usando el atuributo             */
/* RTC_DATA_ATTR.                                                       */
/*                                                                      */
/* Situacion similar para la variable "restart_counter"                 */
/************************************************************************/
static uint32_t RTC_DATA_ATTR restart_counter = 0;

// Espacio de memoria para alojar la propiedad "float *temp;" del objeto.
float RTC_DATA_ATTR temp;

// Espacio de memoria para alojar la propiedad "unsigned char* temp_string;" del objeto.
char temp_string[10];

/************************************************************************/
/* Convierte la temperatura almacenada en float, a cadena de caracteres */
/* Formatea la cadena de texto para que se envie siempre la misma       */
/* cantidad de caracteres, con formato "TMP##.#"                        */
/************************************************************************/
static void convert_temp_to_string(void)
{
    ESP_LOGI(SENSOR_LOG_TAG, "Ingresa a convert_temp_to_string().");
    if (temp >= 100)
    {
        ESP_LOGE(SENSOR_LOG_TAG, "Temperatura supera limite superior.");
        temp = 99.9;
    }
    if (temp <= 0)
    {
        ESP_LOGE(SENSOR_LOG_TAG, "Temperatura supera limite inferior.");
        temp = 0;
    }
    // Convierta a cadena de texto con formato de 4 digitos, 1 posicion decimal.
    snprintf((char *)temp_string, sizeof(temp_string), "%04.1f", temp);
}

/************************************************************************/
/* Simula el sensor de temperatura, generando un desvio positivo o      */
/* negativo en base al resultado de un random.                          */
/* Lo implementé de este modo para que se vaya formando una curva y no  */
/* puntos inconexos                                                     */
/************************************************************************/
static void sample_temp(void)
{
    ESP_LOGI(SENSOR_LOG_TAG, "Ingresa a sample_temp().");

    // Tomo la muestra del supuesto sensor, en este ejemplo solo genero un random.
    ESP_LOGI(SENSOR_LOG_TAG, "Tomando muestra... ");
    uint32_t random_number = esp_random();
    if (random_number > 2147483648)
        temp += 0.3;
    else
        temp -= 0.3;

    if (temp > 40)
        temp = 39.5;
    if (temp < 1)
        temp = 1.5;
    convert_temp_to_string();
}

/************************************************************************/
/* Implementa una funcion de TAREA que se ejecutra en el SO cada vez    */
/* que se la lanza, por unica vez (sin bucle infinito)                  */
/*                                                                      */
/* Lleva al dispositivo a estado de DEEP SLEEP (bajo consuom) tras los  */
/* segundos especificados como parámetro y lo configura para que se     */
/* reinicie tras 30 segundos.                                           */
/*                                                                      */
/* Recibe un uint8_t como parametro tipo "void" y luego lo castea al    */
/* tipo correcto. Las funciones que implementan tareas, reciben         */
/* argumentos de esta manera.                                           */
/************************************************************************/
void go_sleep_task(void *param)
{
    uint8_t seconds = *((uint8_t *)param);
    ESP_LOGI(SENSOR_LOG_TAG, "Inicia go_sleep_task().");
    ESP_LOGI(SENSOR_LOG_TAG, "Segundos para ir a sleep: %d", (int)seconds);
    vTaskDelay(seconds * 1000 / portTICK_PERIOD_MS);

    esp_sleep_enable_timer_wakeup(30 * 1000000);
    ESP_LOGI(SENSOR_LOG_TAG, "Sleep!");
    esp_deep_sleep_start();
    vTaskDelete(NULL);
}

/************************************************************************/
/* Dispara la tarea especificada en xTaskCreate                         */
/*                                                                      */
/* Para disparar una tarea, como primer parametro de xTaskCreate le     */
/* pasamos el puntero a la funcion (su "nombre")                        */
/*                                                                      */
/* Se llama en el manejador de eventos BLE, cuando inicia el ADV        */
/************************************************************************/
static void go_sleep(uint8_t seconds)
{
    // uint8_t *seconds_pointer = &seconds;
    ESP_LOGI(SENSOR_LOG_TAG, "Ingresa a go_sleep().");
    xTaskCreate(go_sleep_task, "go_sleep_task", 4096 * 1, (void *)(&seconds), 3, NULL);
}

/************************************************************************/
/* Inicializa el "objeto"                                               */
/* Lo implementado dentro del IF, se reinicia unicamente cuando el      */
/* dispositivo es energizado, luego, restart_counter será mayor a 0.    */
/* Esto es útil cuando tenemos que inicializar algo en el dispositivo   */
/* por única vez cuando enciende.                                       */
/************************************************************************/
static void initialize(void)
{
    ESP_LOGI(SENSOR_LOG_TAG, "Ingresa a initialize().");
    if (restart_counter == 0)
    {
        ESP_LOGI(SENSOR_LOG_TAG, "Inicializa temperatura a 24.");
        temp = 24;
    }
    ESP_LOGI(SENSOR_LOG_TAG, "Reinicio numero: %d", (int)restart_counter);
    restart_counter++;
}

const char *mqtt_topic = NULL;
const char *mqtt_deviceId = NULL;
esp_mqtt_client_handle_t *esp_mqtt_client_handle = NULL;

static void set_mqtt_info(const char *topic, const char *deviceId, esp_mqtt_client_handle_t *esp_mqtt_client_handle_pointer)
{
    esp_mqtt_client_handle = esp_mqtt_client_handle_pointer;
    mqtt_topic = topic;
    mqtt_deviceId = deviceId;
}

static void publish_to_mqtt(void)
{
    ESP_LOGI(SENSOR_LOG_TAG, "Ingresa a publish_to_mqtt_topic()");

    char bufferJson[300];
    char bufferTopic[350];
    int msg_id;
    char buffer_temp_txt[15];
    char buffer_rssi_txt[15];

    // Consulto al modulo el nivel de señal que esta recibiendo.
    int8_t rssi = 0;
    wifi_ap_record_t ap_info;
    esp_wifi_sta_get_ap_info(&ap_info);
    rssi = ap_info.rssi;
    snprintf(buffer_rssi_txt, sizeof(buffer_rssi_txt), "%d", rssi);

    bufferJson[0] = 0;

    strcat(bufferJson, "{ \"dev_id\": ");
    strcat(bufferJson, mqtt_deviceId + strlen(mqtt_deviceId) - 3); // " + strlen(mqtt_deviceId) - 3", como trabajo con punteros, equivale a string.right(3)
    strcat(bufferJson, ", \"temperatura\": ");
    strcat(bufferJson, temp_string);
    strcat(bufferJson, ", \"rssi\": ");
    strcat(bufferJson, buffer_rssi_txt);
    strcat(bufferJson, " }");

    ESP_LOGI(SENSOR_LOG_TAG, "JSON enviado:  %s", bufferJson);

    bufferTopic[0] = 0;
    strcat(bufferTopic, "/devices/");
    strcat(bufferTopic, mqtt_deviceId);

    // Ejemplo para publicar telemetria (eventos) telemetria por defecto, derivada a un topic Google pub/sub
    //  Asi publico a una "subcarpeta", declarada en Clearblade y redirigida a un TOPIC de Google pub/sub
    strcat(bufferTopic, "/events");
    msg_id = esp_mqtt_client_publish(*esp_mqtt_client_handle, bufferTopic, bufferJson, 0, 1, 0);

    // Ejemplo para publicar telemetria (eventos) subcarpeta
    // Asi publico a una "subcarpeta", declarada en Clearblade y redirigida a un TOPIC de Google pub/sub
    // strcat(bufferTopic, "/events/bmp");
    // msg_id = esp_mqtt_client_publish(cliente, bufferTopic, bufferJson, 0, 1, 0);

    // Ejemplo para publicar y actualizar STATE en IoT Core
    // TOPIC: /devices/DEVICE-ID/state
    // strcat(bufferTopic, "/state");
    // msg_id = esp_mqtt_client_publish(cliente, bufferTopic, "state desde device-101", 0, 1, 0);

    ESP_LOGI(SENSOR_LOG_TAG, "sent publish successful, msg_id=%d", msg_id);
}

/*****************************************************
 *   Driver Instance Declaration(s) API(s)
 ******************************************************/
const tempSensor_t tempSensor = {
    // Sensor Props
    .temp_string = temp_string,
    .temp = &temp,
    // Sensor Functions
    .initialize = initialize,
    .sample_temp = sample_temp,
    .go_sleep = go_sleep,
    .set_mqtt_info = set_mqtt_info,
    .publish_to_mqtt = publish_to_mqtt,
};
