/*
 * sntp_time.c
 *
 *  Created on: 16/3/2020
 *      Author: Leopoldo Zimperz
 *      Mail: leopoldo.a.zimperz@gmail.com
 *
 */

#include "sntp_time.h"
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_sntp.h"
#include "clearblade_connect.h"

static const char *TAG = "SNTP Module";

/* Variable holding number of times ESP32 restarted since first boot.
 * It is placed into RTC memory using RTC_DATA_ATTR and
 * maintains its value when ESP32 wakes from deep sleep.
 */
RTC_DATA_ATTR static int boot_count = 0;

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
    xEventGroupSetBits(*mqtt_client.mqtt_event_group, TIME_SYNCHRONIZED);
}

void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "time.google.com");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
}
