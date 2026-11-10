#pragma once
#include "esp_err.h"
#include "esp_netif_ip_addr.h"
typedef struct { int unused; } esp_netif_t;
enum { IP_EVENT_ASSIGNED_IP_TO_CLIENT = 20 };
typedef struct {
    esp_netif_t *esp_netif;
    esp_ip4_addr_t ip;
    uint8_t mac[6];
} ip_event_assigned_ip_to_client_t;
esp_err_t esp_netif_init(void);
esp_netif_t *esp_netif_create_default_wifi_ap(void);
