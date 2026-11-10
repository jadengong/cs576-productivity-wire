#pragma once
#include <stdbool.h>
#include "esp_err.h"
typedef struct {
    int event_id;
    void *user_data;
    const char *header_key;
    const char *header_value;
    void *data;
    int data_len;
} esp_http_client_event_t;
enum { HTTP_EVENT_ON_HEADER, HTTP_EVENT_ON_DATA };
enum { HTTP_METHOD_GET, HTTP_METHOD_POST };
typedef struct {
    const char *url;
    int method;
    int timeout_ms;
    bool disable_auto_redirect;
    esp_err_t (*event_handler)(esp_http_client_event_t *);
    void *user_data;
} esp_http_client_config_t;
typedef struct fake_client *esp_http_client_handle_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *name, const char *value);
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t client, const char *data, int size);
esp_err_t esp_http_client_perform(esp_http_client_handle_t client);
int esp_http_client_get_status_code(esp_http_client_handle_t client);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client);
