#pragma once
#include <stdbool.h>
#include "esp_event.h"
enum { WIFI_EVENT_AP_STACONNECTED, WIFI_EVENT_AP_STADISCONNECTED, WIFI_EVENT_AP_STOP };
enum { WIFI_MODE_AP, WIFI_IF_AP, WIFI_AUTH_OPEN, WIFI_AUTH_WPA2_PSK };
typedef struct { uint8_t mac[6]; } wifi_event_ap_staconnected_t;
typedef struct { uint8_t mac[6]; } wifi_event_ap_stadisconnected_t;
typedef struct { int unused; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
typedef struct {
    struct { uint8_t ssid[32], ssid_len, password[64]; int channel, authmode, max_connection; } ap;
} wifi_config_t;
esp_err_t esp_wifi_init(const wifi_init_config_t *config);
esp_err_t esp_wifi_set_mode(int mode);
esp_err_t esp_wifi_set_config(int interface, const wifi_config_t *config);
esp_err_t esp_wifi_start(void);
