/*
 * wifi_manager.h
 *
 *  Created on: 14/9/2023
 *      Author: Leopoldo Zimperz
 *      Mail: leopoldo.a.zimperz@gmail.com
 *
 */

#ifndef LZ_WIFI_MANAGER_
#define LZ_WIFI_MANAGER_

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/************************************************************************/
/* La siguiente estructura simula un objeto en C                        */
/*                                                                      */
/* Las propiedades estan implementadas comp punteros a variables del .c */
/* Los metodos son punteros a funciones definidas dentro del .c         */
/************************************************************************/
typedef struct
{
    // Wi-Fi Manager Props
    EventGroupHandle_t *wifi_event_group;
    // Wi-Fi Manager Functions
    void (*wifi_init)(void);
    void (*set_sta_credentials)(char *ssid, char *pass);
    char *(*get_sta_ssid)(void);
    char *(*get_sta_ip)(void);
    void (*set_got_ip_callback)(void *callback);
    void (*set_ap_ip)(char *ip);
} wifi_manager_t;

/************************************************************************/
/* La declaración de esta variable se realiza dentro del .c             */
/*                                                                      */
/* Agregar este "extern" en el archivo de cabecera, permite que         */
/* cualquier .c que lo incluya con #include, tenga acceso al "objeto".  */
/************************************************************************/
extern const wifi_manager_t wifi_manager;

#endif /* LZ_WIFI_MANAGER_ */