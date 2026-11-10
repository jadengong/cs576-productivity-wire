#include "network.h"
#include "reporter.h"

#include <string.h>

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

// Kconfi.projbuild located within the same folder contains the configuration
// Network SSID & PASSWORD
_Static_assert(sizeof(CONFIG_NETSERVER_WIFI_SSID) > 1 &&
               sizeof(CONFIG_NETSERVER_WIFI_SSID) <= 33,
               "Wi-Fi SSID must contain 1 to 32 bytes");
_Static_assert(sizeof(CONFIG_NETSERVER_WIFI_PASSWORD) == 1 ||
               (sizeof(CONFIG_NETSERVER_WIFI_PASSWORD) >= 9 &&
                sizeof(CONFIG_NETSERVER_WIFI_PASSWORD) <= 64),
               "Wi-Fi password must be empty or contain 8 to 63 bytes");

// Initializing a few variables
static const char *TAG = "network";
static esp_netif_t *access_point;
static bool host_selected;
static uint8_t host_mac[6];

static void network_event_handler(void *argument, esp_event_base_t base,
                                   int32_t event_id, void *event_data)
{
    (void)argument;
    /* Both handlers run on the default event loop, which serializes host selection. */
    /* First client to join it's DHCP address is saved and selected as the host for the webserver */
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *event = event_data;
        if (!host_selected) {
            memcpy(host_mac, event->mac, sizeof(host_mac));
            host_selected = true;
            ESP_LOGI(TAG, "Selected first client " MACSTR "; waiting for its DHCP address",
                     MAC2STR(host_mac));
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_ASSIGNED_IP_TO_CLIENT) {
        const ip_event_assigned_ip_to_client_t *event = event_data;
        if (event->esp_netif == access_point && host_selected &&
            memcmp(host_mac, event->mac, sizeof(host_mac)) == 0 && event->ip.addr != 0) {
            reporter_set_host(&event->ip);
        }
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *event = event_data;
        if (host_selected && memcmp(host_mac, event->mac, sizeof(host_mac)) == 0) {
            host_selected = false;
            reporter_set_host(NULL);
            // In the case the first client disconnects
            ESP_LOGI(TAG, "Server client disconnected; waiting for the next new connection");
        }
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
        host_selected = false;
        reporter_set_host(NULL);
    }
}

void network_start(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    access_point = esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               network_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ASSIGNED_IP_TO_CLIENT,
                                               network_event_handler, NULL));

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_NETSERVER_WIFI_SSID,
            .ssid_len = sizeof(CONFIG_NETSERVER_WIFI_SSID) - 1,
            .password = CONFIG_NETSERVER_WIFI_PASSWORD,
            .channel = CONFIG_NETSERVER_WIFI_CHANNEL,
            .authmode = sizeof(CONFIG_NETSERVER_WIFI_PASSWORD) == 1
                            ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK,
            .max_connection = 4,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi access point: %s", CONFIG_NETSERVER_WIFI_SSID);
    ESP_LOGI(TAG, "Waiting for the first client's API on port %d", CONFIG_NETSERVER_SERVER_PORT);
}
