#pragma once
#include "esp_err.h"
enum { ESP_ERR_NVS_NO_FREE_PAGES = 100, ESP_ERR_NVS_NEW_VERSION_FOUND };
esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_erase(void);
