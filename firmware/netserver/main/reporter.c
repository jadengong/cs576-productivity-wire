#include "reporter.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>

#include "sdkconfig.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

_Static_assert(sizeof(CONFIG_NETSERVER_DEVICE_ID) > 1 &&
               sizeof(CONFIG_NETSERVER_DEVICE_ID) <= 101,
               "Device ID must contain 1 to 100 bytes");

static const char *TAG = "reporter";

typedef struct {
    esp_ip4_addr_t address;
    unsigned int generation;
    bool ready;
} host_t;

typedef struct {
    time_t timestamp;
    unsigned int generation;
} trigger_t;

static portMUX_TYPE host_lock = portMUX_INITIALIZER_UNLOCKED;
static host_t selected_host;
static QueueHandle_t trigger_queue;

static host_t get_host(void)
{
    portENTER_CRITICAL(&host_lock);
    host_t host = selected_host;
    portEXIT_CRITICAL(&host_lock);
    return host;
}

void reporter_set_host(const esp_ip4_addr_t *address)
{
    uint32_t ip = address ? address->addr : 0;
    portENTER_CRITICAL(&host_lock);
    bool changed = selected_host.address.addr != ip;
    if (changed) {
        selected_host.address.addr = ip;
        ++selected_host.generation;
        selected_host.ready = false;
    }
    portEXIT_CRITICAL(&host_lock);
    if (changed && address != NULL) {
        ESP_LOGI(TAG, "Server selected: http://" IPSTR ":%d",
                 IP2STR(address), CONFIG_NETSERVER_SERVER_PORT);
    }
}

bool reporter_is_ready(void)
{
    return get_host().ready;
}

typedef struct {
    char body[384];
    size_t used;
    char date[64];
} server_response_t;

static esp_err_t response_handler(esp_http_client_event_t *event)
{
    server_response_t *response = event->user_data;
    if (event->event_id == HTTP_EVENT_ON_HEADER &&
        strcasecmp(event->header_key, "Date") == 0) {
        snprintf(response->date, sizeof(response->date), "%s", event->header_value);
    } else if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
        size_t count = event->data_len;
        size_t available = sizeof(response->body) - response->used - 1;
        if (count > available) {
            count = available;
        }
        memcpy(response->body + response->used, event->data, count);
        response->used += count;
        response->body[response->used] = '\0';
    }
    return ESP_OK;
}

static esp_err_t request_server(const esp_ip4_addr_t *address, const char *path, const char *payload,
                                int expected_status, server_response_t *response)
{
    char url[64];
    int length = snprintf(url, sizeof(url), "http://" IPSTR ":%d%s",
                          IP2STR(address), CONFIG_NETSERVER_SERVER_PORT, path);
    if (length < 0 || (size_t)length >= sizeof(url)) {
        return ESP_ERR_INVALID_SIZE;
    }

    esp_http_client_config_t config = {
        .url = url,
        .method = payload ? HTTP_METHOD_POST : HTTP_METHOD_GET,
        .timeout_ms = CONFIG_NETSERVER_REQUEST_TIMEOUT_MS,
        .disable_auto_redirect = true,
        .event_handler = response_handler,
        .user_data = response,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ESP_OK;
    if (payload != NULL) {
        err = esp_http_client_set_header(client, "Content-Type", "application/json");
        if (err == ESP_OK) {
            err = esp_http_client_set_post_field(client, payload, strlen(payload));
        }
    }
    if (err == ESP_OK) {
        err = esp_http_client_perform(client);
    }
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s failed: %s", path, esp_err_to_name(err));
        return err;
    }
    if (status != expected_status) {
        ESP_LOGE(TAG, "%s returned HTTP %d: %s", path, status, response->body);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t validate_device_id(void)
{
    /* Restrict IDs to ASCII identifiers so they can safely be placed in JSON. */
    for (const char *p = CONFIG_NETSERVER_DEVICE_ID; *p; ++p) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.')) {
            ESP_LOGE(TAG, "Device ID may contain only letters, digits, '.', '_' and '-'");
            return ESP_ERR_INVALID_ARG;
        }
    }
    return ESP_OK;
}

// Little health check on if the designated host is running a websever
static esp_err_t prepare_server(const esp_ip4_addr_t *address)
{
    server_response_t response = {0};
    esp_err_t err = request_server(address, "/health", NULL, 200, &response);
    if (err != ESP_OK) {
        return err;
    }

    /* Uvicorn sends this header by default; this also works without internet/NTP. */
    struct tm utc = {0};
    char *end = strptime(response.date, "%a, %d %b %Y %H:%M:%S GMT", &utc);
    if (end == NULL || *end != '\0' || utc.tm_year < 124) {
        ESP_LOGE(TAG, "Server must return a valid HTTP Date header (enable Uvicorn date headers)");
        return ESP_ERR_INVALID_RESPONSE;
    }
    struct timeval now = {.tv_sec = timegm(&utc)};
    if (settimeofday(&now, NULL) != 0) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Server reachable; clock synchronized to %s", response.date);
    return ESP_OK;
}

// Talks with the webserver on the the corresponding host
static esp_err_t send_trigger(const esp_ip4_addr_t *address, time_t timestamp)
{
    struct tm utc;
    char formatted_time[21];
    if (gmtime_r(&timestamp, &utc) == NULL || utc.tm_year < 124 ||
        strftime(formatted_time, sizeof(formatted_time), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    char payload[192];
    int length = snprintf(payload, sizeof(payload),
                          "{\"device_id\":\"%s\",\"event\":\"triggered\",\"timestamp\":\"%s\"}",
                          CONFIG_NETSERVER_DEVICE_ID, formatted_time);
    if (length < 0 || (size_t)length >= sizeof(payload)) {
        return ESP_ERR_INVALID_SIZE;
    }

    server_response_t response = {0};
    esp_err_t err = request_server(address, "/api/triggers", payload, 202, &response);
    if (err == ESP_OK) {
        /* Acceptance/delivery are distinct from execution on the desktop client. */
        ESP_LOGI(TAG, "Trigger accepted: %s", response.body);
    }
    return err;
}

static void reporting_task(void *argument)
{
    (void)argument;
    unsigned int generation = 0;
    bool probe_immediately = true;
    TickType_t last_probe = 0;

    // big infinite loop
    for (;;) {
        // Logic that first host is chosen as the one hosting the webserver
        host_t host = get_host();
        if (host.generation != generation) {
            generation = host.generation;
            probe_immediately = true;
        }
        if (host.address.addr != 0 && !host.ready &&
            (probe_immediately || xTaskGetTickCount() - last_probe >=
                                  pdMS_TO_TICKS(CONFIG_NETSERVER_SERVER_RETRY_MS))) {
            probe_immediately = false;
            esp_err_t err = prepare_server(&host.address);
            last_probe = xTaskGetTickCount();
            portENTER_CRITICAL(&host_lock);
            bool still_selected = selected_host.generation == host.generation;
            if (still_selected && err == ESP_OK) {
                selected_host.ready = true;
            }
            portEXIT_CRITICAL(&host_lock);
            if (still_selected && err == ESP_OK) {
                ESP_LOGI(TAG, "Reporting armed for %s", CONFIG_NETSERVER_DEVICE_ID);
            }
        }

        // pre-emptive trigger behavior for the IR break beam
        trigger_t trigger;
        if (xQueueReceive(trigger_queue, &trigger, pdMS_TO_TICKS(250)) != pdTRUE) {
            continue;
        }
        host = get_host();
        if (!host.ready || trigger.generation != host.generation) {
            continue;
        }
        esp_err_t err = send_trigger(&host.address, trigger.timestamp);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Trigger not confirmed: %s; checking server before accepting more",
                     esp_err_to_name(err));
            portENTER_CRITICAL(&host_lock);
            if (selected_host.generation == host.generation) {
                selected_host.ready = false;
                ++selected_host.generation;
            }
            portEXIT_CRITICAL(&host_lock);
            /* No POST retries: the API has no deduplication and may already have
             * delivered a command. The new generation discards queued old events. */
        }
    }
}


esp_err_t reporter_init(void)
{
    if (trigger_queue != NULL) {
        return ESP_OK;
    }
    esp_err_t err = validate_device_id();
    if (err != ESP_OK) {
        return err;
    }
    trigger_queue = xQueueCreate(8, sizeof(trigger_t));
    if (trigger_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(reporting_task, "trigger_reporter", 8192, NULL, 5, NULL) != pdPASS) {
        vQueueDelete(trigger_queue);
        trigger_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t reporter_trigger(void)
{
    host_t host = get_host();
    if (trigger_queue == NULL || !host.ready) {
        return ESP_ERR_INVALID_STATE;
    }
    trigger_t trigger = {.timestamp = time(NULL), .generation = host.generation};
    return xQueueSend(trigger_queue, &trigger, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
