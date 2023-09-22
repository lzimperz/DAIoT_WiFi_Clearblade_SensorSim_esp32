/*
 * wifi_manager_miscs.h
 *
 *  Created on: 14/9/2023
 *      Author: Leopoldo Zimperz
 *      Mail: leopoldo.a.zimperz@gmail.com
 *
 */

#ifndef WIFI_MANAGER_MISCS_
#define WIFI_MANAGER_MISCS_

#include "esp_err.h"

esp_err_t get_config_param_str(char* name, char** param);
esp_err_t set_config_param_str(char* name, char* param);








#endif /* WIFI_MANAGER_MISCS_ */