/*
 * wifi_manager.c
 *
 *  Created on: 14/9/2023
 *      Author: Leopoldo Zimperz
 *      Mail: leopoldo.a.zimperz@gmail.com
 *
 */

#include <string.h>
#include "wifi_manager.h"
#include "wfm_miscs.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "lwip/lwip_napt.h"
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_system.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "esp_system.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif_types.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif_types.h"
#include "esp_netif_defaults.h"

#define WIFI_STA_MAXIMUM_CONNECT_RETRY 5

#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""

#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

#define WIFI_AP_SSID "ESP_CONFIG_AP"
#define WIFI_AP_PASS "12345678"
#define WIFI_AP_CHANNEL 8
#define WIFI_AP_MAX_STA_CONN 1
#define DEFAULT_AP_IP "192.168.100.100"

void wifi_init(void);

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_STA_CONNECTED_BIT BIT0
#define WIFI_STA_FAIL_BIT BIT1

static const char *TAG = "wifi module";

static int s_retry_num = 0;
char *sta_ssid = NULL;
char *sta_pass = NULL;
char *sta_ip = NULL;
char *ap_ip = DEFAULT_AP_IP;
void (*event_got_ip_callback)(void);

esp_netif_t *wifiAP;
esp_netif_t *wifiSTA;

inline static void wifi_init_sta(void);
inline static void wifi_init_softap(void);
inline static void wifi_wait_for_ip(void);
void set_ap_ip(char *ip);

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < WIFI_STA_MAXIMUM_CONNECT_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(wifi_event_group, WIFI_STA_FAIL_BIT);
            xEventGroupClearBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        sta_ip = malloc(20);
        sprintf(sta_ip, IPSTR, IP2STR(&event->ip_info.ip));
        event_got_ip_callback();
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);
        xEventGroupClearBits(wifi_event_group, WIFI_STA_FAIL_BIT);
    }
    else
    {
        ESP_LOGI(TAG, "Evento sin tratar %d %d", (int)event_base, (int)event_id);
    }
}

inline static void wifi_init_sta(void)
{
    wifiSTA = esp_netif_create_default_wifi_sta();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        // IP_EVENT_STA_GOT_IP,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            //.ssid = sta_ssid,
            //.password = sta_pass,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };

    if (strlen(sta_ssid) > 0)
    {
        strlcpy((char *)wifi_config.sta.ssid, sta_ssid, sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, sta_pass, sizeof(wifi_config.sta.password));
    }
    else
    {
        ESP_LOGE(TAG, "SSID or PASS config error.");
        esp_restart();
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    xTaskCreate(wifi_wait_for_ip, "wifi_wait_for_ip_task", 2048, NULL, 10, NULL);
}

inline static void wifi_wait_for_ip(void)
{
    /* Waiting until either the connection is established (WIFI_STA_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_STA_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_STA_CONNECTED_BIT | WIFI_STA_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_STA_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 sta_ssid, sta_pass);
    }
    else if (bits & WIFI_STA_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 sta_ssid, sta_pass);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    vTaskDelete(NULL);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else
    {
        ESP_LOGI(TAG, "Evento WIFI sin tratar %d %d", (int)event_base, (int)event_id);
    }
}

inline static void wifi_init_softap(void)
{
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifiAP = esp_netif_create_default_wifi_ap();

    set_ap_ip(DEFAULT_AP_IP);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASS,
            .max_connection = WIFI_AP_MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                .required = true,
            },
        },
    };
    if (strlen(WIFI_AP_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL);
}

void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();
    event_got_ip_callback = NULL;

    ESP_ERROR_CHECK(esp_netif_init());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    get_config_param_str("sta_ssid", &sta_ssid);
    get_config_param_str("sta_pass", &sta_pass);
    if (sta_ssid == NULL || sta_pass == NULL)
    {
        ESP_LOGI(TAG, "WiFi credentials not set.");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    }
    else
    {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_LOGI(TAG, "ESP_WIFI_MODE_STA CONFIG");
        wifi_init_sta();
    }

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP CONFIG");
    wifi_init_softap();

    ESP_LOGI(TAG, "ESP_WIFI STARTING...");
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void set_sta_credentials(char *ssid, char *pass)
{

    if (ssid == NULL || pass == NULL)
        return;
    if (strlen(ssid) == 0)
        return;
    if (strlen(pass) == 0)
        return;

    set_config_param_str("sta_ssid", ssid);
    set_config_param_str("sta_pass", pass);

    get_config_param_str("sta_ssid", &sta_ssid);
    get_config_param_str("sta_pass", &sta_pass);

    // esp_restart();
}

char *get_sta_ssid(void)
{
    return sta_ssid;
}

char *get_sta_ip(void)
{
    return sta_ip;
}

void set_got_ip_callback(void *callback)
{

    ESP_LOGI(TAG, "Setting got IP event callback.");
    event_got_ip_callback = callback;
}

void set_ap_ip(char *ip)
{

    uint32_t my_ap_ip = esp_ip4addr_aton(ip);

    esp_netif_ip_info_t ipInfo_ap;
    ipInfo_ap.ip.addr = my_ap_ip;
    ipInfo_ap.gw.addr = my_ap_ip;
    esp_netif_set_ip4_addr(&ipInfo_ap.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(wifiAP); // stop before setting ip WifiAP
    esp_netif_set_ip_info(wifiAP, &ipInfo_ap);
    esp_netif_dhcps_start(wifiAP);
}

/*****************************************************
 *   Driver Instance Declaration(s) API(s)            *
 ******************************************************/
const wifi_manager_t wifi_manager = {
    // Wi-Fi Manager Props
    .wifi_event_group = &wifi_event_group,
    // Wi-Fi Manager Functions
    .wifi_init = wifi_init,
    .set_sta_credentials = set_sta_credentials,
    .get_sta_ssid = get_sta_ssid,
    .get_sta_ip = get_sta_ip,
    .set_got_ip_callback = set_got_ip_callback,
    .set_ap_ip = set_ap_ip,
};