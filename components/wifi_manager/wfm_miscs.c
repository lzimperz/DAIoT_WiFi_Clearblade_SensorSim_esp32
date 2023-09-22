/*
 * wifi_manager_miscs.c
 *
 *  Created on: 14/9/2023
 *      Author: Leopoldo Zimperz
 *      Mail: leopoldo.a.zimperz@gmail.com
 *
 */

#include "wfm_miscs.h"
#include "nvs.h"
#include "esp_log.h"

#define PARAM_NAMESPACE "ESP32_WFM"
static const char *TAG = "wifi module miscs";

esp_err_t get_config_param_str(char *name, char **param)
{
    nvs_handle_t nvs;

    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK)
    {
        size_t len;
        if ((err = nvs_get_str(nvs, name, NULL, &len)) == ESP_OK)
        {
            *param = (char *)malloc(len);
            err = nvs_get_str(nvs, name, *param, &len);
            ESP_LOGI(TAG, "Param readed: %s/%s", name, *param);
        }
        else
        {
            return err;
        }
        nvs_close(nvs);
    }
    else
    {
        return err;
    }
    return ESP_OK;
}

/* 'set_sta' command */
esp_err_t set_config_param_str(char *name, char *param)
{
    esp_err_t err;
    nvs_handle_t nvs;

    ESP_LOGI(TAG, "Opening NVS_READWRITE");
    err = nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    ESP_LOGI(TAG, "NVS Writing param...");
    err = nvs_set_str(nvs, name, param);
    if (err == ESP_OK)
    {
        err = nvs_commit(nvs);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "Param writed: %s/%s", name, param);
        }
    }

    ESP_LOGI(TAG, "Closing NVS_READWRITE");
    nvs_close(nvs);
    return err;
}
