#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef const char *esp_event_base_t;
typedef void (*esp_event_handler_t)(void *, esp_event_base_t, int32_t, void *);
extern esp_event_base_t WIFI_EVENT;
extern esp_event_base_t IP_EVENT;
#define ESP_EVENT_ANY_ID (-1)
esp_err_t esp_event_loop_create_default(void);
esp_err_t esp_event_handler_register(esp_event_base_t base, int32_t event_id,
                                     esp_event_handler_t handler, void *argument);
