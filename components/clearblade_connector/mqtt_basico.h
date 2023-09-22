/*
 * mqtt_basico.h
 *
 *  Created on: 16/3/2020
 *      Author: Leopoldo Zimperz
 *      Mail: leopoldo.a.zimperz@gmail.com
 *
 *  Basado en: ESP-IDF - MQTT (over TCP) Example
 */

#ifndef MAIN_MQTT_BASICO_H_
#define MAIN_MQTT_BASICO_H_

#include "stdlib.h"
#include "stdio.h"

#define BROKER_URI				"mqtts://us-central1-mqtt.clearblade.com"
#define IOTCORE_USERNAME		"unused"
#define IOTCORE_TOKEN_EXPIRATION_TIME_MINUTES		60 * 24

#define ERROR_CODE_RESET	1
#define ERROR_CODE_JWT		2
#define ERROR_CODE_SNTP		4
#define ERROR_CODE_MQTT		8
#define ERROR_CODE_TIMEOUT  16
#define ERROR_CODE_WIFI  	32
#define ERROR_CODE_IP		64

void mqtt_app_main_task(void * parm);

extern int last_error_count;
extern int last_error_code;
extern unsigned int tph_on_time;
extern unsigned int last_on_time_seconds;
extern int last_sntp_response_time_seconds;

#endif /* MAIN_MQTT_BASICO_H_ */
