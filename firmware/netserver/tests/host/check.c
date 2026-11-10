#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "network.h"
#include "reporter.h"

esp_event_base_t WIFI_EVENT = "wifi";
esp_event_base_t IP_EVENT = "ip";
static esp_netif_t ap;
static esp_event_handler_t wifi_handler, ip_handler;
static atomic_int health_status = 503, post_result = ESP_OK;
static atomic_int health_requests, posts_started, posts_completed;
static atomic_bool block_post, omit_date;
static atomic_long clock_seconds;
static pthread_mutex_t observation_lock = PTHREAD_MUTEX_INITIALIZER;
static char last_health_url[80], last_post_url[80], last_payload[256];

esp_err_t nvs_flash_init(void) { return ESP_OK; }
esp_err_t nvs_flash_erase(void) { return ESP_OK; }
esp_err_t esp_netif_init(void) { return ESP_OK; }
esp_err_t esp_event_loop_create_default(void) { return ESP_OK; }
esp_netif_t *esp_netif_create_default_wifi_ap(void) { return &ap; }
esp_err_t esp_wifi_init(const wifi_init_config_t *config) { (void)config; return ESP_OK; }
esp_err_t esp_wifi_set_mode(int mode) { assert(mode == WIFI_MODE_AP); return ESP_OK; }
esp_err_t esp_wifi_set_config(int interface, const wifi_config_t *config)
{
    assert(interface == WIFI_IF_AP && config->ap.max_connection == 4);
    return ESP_OK;
}
esp_err_t esp_wifi_start(void) { return ESP_OK; }
esp_err_t esp_event_handler_register(esp_event_base_t base, int32_t id,
                                     esp_event_handler_t handler, void *argument)
{
    (void)id; (void)argument;
    if (base == WIFI_EVENT) wifi_handler = handler;
    else if (base == IP_EVENT) ip_handler = handler;
    else assert(0);
    return ESP_OK;
}

struct fake_queue {
    pthread_mutex_t lock;
    pthread_cond_t available;
    unsigned capacity, size, count;
    unsigned char *data;
};
QueueHandle_t xQueueCreate(unsigned capacity, unsigned size)
{
    QueueHandle_t queue = calloc(1, sizeof(*queue));
    assert(queue);
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->available, NULL);
    queue->capacity = capacity;
    queue->size = size;
    queue->data = calloc(capacity, size);
    assert(queue->data);
    return queue;
}
int xQueueSend(QueueHandle_t queue, const void *value, TickType_t wait)
{
    assert(wait == 0);
    pthread_mutex_lock(&queue->lock);
    int result = 0;
    if (queue->count < queue->capacity) {
        memcpy(queue->data + queue->count++ * queue->size, value, queue->size);
        pthread_cond_signal(&queue->available);
        result = pdTRUE;
    }
    pthread_mutex_unlock(&queue->lock);
    return result;
}
int xQueueReceive(QueueHandle_t queue, void *value, TickType_t wait)
{
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += wait / 1000;
    deadline.tv_nsec += (long)(wait % 1000) * 1000000;
    if (deadline.tv_nsec >= 1000000000) { ++deadline.tv_sec; deadline.tv_nsec -= 1000000000; }
    pthread_mutex_lock(&queue->lock);
    while (queue->count == 0) {
        int err = pthread_cond_timedwait(&queue->available, &queue->lock, &deadline);
        if (err == ETIMEDOUT) break;
        assert(err == 0);
    }
    int result = 0;
    if (queue->count > 0) {
        memcpy(value, queue->data, queue->size);
        --queue->count;
        memmove(queue->data, queue->data + queue->size, queue->count * queue->size);
        result = pdTRUE;
    }
    pthread_mutex_unlock(&queue->lock);
    return result;
}
void vQueueDelete(QueueHandle_t queue)
{
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->available);
    free(queue->data);
    free(queue);
}
static struct { void (*function)(void *); void *argument; } task;
static void *run_task(void *argument) { (void)argument; task.function(task.argument); return NULL; }
int xTaskCreate(void (*function)(void *), const char *name, unsigned stack,
                void *argument, unsigned priority, void *handle)
{
    (void)name; (void)priority; (void)handle;
    assert(stack >= 8192);
    task.function = function;
    task.argument = argument;
    pthread_t thread;
    assert(pthread_create(&thread, NULL, run_task, NULL) == 0);
    pthread_detach(thread);
    return pdPASS;
}
TickType_t xTaskGetTickCount(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (TickType_t)(now.tv_sec * 1000 + now.tv_nsec / 1000000);
}
time_t mock_time(time_t *output)
{
    time_t value = atomic_load(&clock_seconds);
    if (output) *output = value;
    return value;
}
int mock_settimeofday(const struct timeval *now, const struct timezone *zone)
{
    (void)zone;
    assert(now->tv_sec == 1793188800);
    atomic_store(&clock_seconds, now->tv_sec);
    return 0;
}

struct fake_client { esp_http_client_config_t config; char *payload; int status; };
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config)
{
    assert(config->disable_auto_redirect && config->timeout_ms == 100);
    esp_http_client_handle_t client = calloc(1, sizeof(*client));
    assert(client);
    client->config = *config;
    return client;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *name, const char *value)
{
    (void)client;
    assert(!strcmp(name, "Content-Type") && !strcmp(value, "application/json"));
    return ESP_OK;
}
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t client, const char *data, int size)
{
    assert((size_t)size == strlen(data));
    client->payload = strdup(data);
    assert(client->payload);
    return ESP_OK;
}
esp_err_t esp_http_client_perform(esp_http_client_handle_t client)
{
    bool health = client->config.method == HTTP_METHOD_GET;
    pthread_mutex_lock(&observation_lock);
    if (health) {
        snprintf(last_health_url, sizeof(last_health_url), "%s", client->config.url);
    } else {
        assert(client->config.method == HTTP_METHOD_POST);
        snprintf(last_post_url, sizeof(last_post_url), "%s", client->config.url);
        snprintf(last_payload, sizeof(last_payload), "%s", client->payload);
    }
    pthread_mutex_unlock(&observation_lock);
    if (health) atomic_fetch_add(&health_requests, 1);
    else {
        atomic_fetch_add(&posts_started, 1);
        while (atomic_load(&block_post)) usleep(1000);
    }
    client->status = health ? atomic_load(&health_status) : 202;
    if (!atomic_load(&omit_date)) {
        esp_http_client_event_t event = {
            .event_id = HTTP_EVENT_ON_HEADER, .user_data = client->config.user_data,
            .header_key = "dAtE", .header_value = "Wed, 28 Oct 2026 12:00:00 GMT",
        };
        assert(client->config.event_handler(&event) == ESP_OK);
    }
    char *chunks[] = {"{\"status\":\"accepted\",", "\"delivery\":\"sent\"}"};
    for (size_t i = 0; i < 2; ++i) {
        esp_http_client_event_t event = {
            .event_id = HTTP_EVENT_ON_DATA, .user_data = client->config.user_data,
            .data = chunks[i], .data_len = strlen(chunks[i]),
        };
        assert(client->config.event_handler(&event) == ESP_OK);
    }
    if (!health) atomic_fetch_add(&posts_completed, 1);
    return health ? ESP_OK : atomic_load(&post_result);
}
int esp_http_client_get_status_code(esp_http_client_handle_t client) { return client->status; }
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client)
{
    free(client->payload);
    free(client);
    return ESP_OK;
}

static void connect_client(unsigned client)
{
    wifi_event_ap_staconnected_t event = {.mac = {0, 0, 0, 0, 0, client}};
    wifi_handler(NULL, WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &event);
}
static void disconnect_client(unsigned client)
{
    wifi_event_ap_stadisconnected_t event = {.mac = {0, 0, 0, 0, 0, client}};
    wifi_handler(NULL, WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &event);
}
static void assign_ip(unsigned client, const char *ip)
{
    ip_event_assigned_ip_to_client_t event = {.esp_netif = &ap, .mac = {0, 0, 0, 0, 0, client}};
    assert(inet_pton(AF_INET, ip, &event.ip.addr) == 1);
    ip_handler(NULL, IP_EVENT, IP_EVENT_ASSIGNED_IP_TO_CLIENT, &event);
}
static void wait_count(atomic_int *value, int expected)
{
    for (int i = 0; i < 3000 && atomic_load(value) < expected; ++i) usleep(1000);
    assert(atomic_load(value) >= expected);
}
static void wait_ready(void)
{
    for (int i = 0; i < 3000 && !reporter_is_ready(); ++i) usleep(1000);
    assert(reporter_is_ready());
}
static void expect_post_url(const char *url)
{
    pthread_mutex_lock(&observation_lock);
    assert(!strcmp(last_post_url, url));
    pthread_mutex_unlock(&observation_lock);
}

int main(void)
{
    assert(reporter_trigger() == ESP_ERR_INVALID_STATE);
    assert(reporter_init() == ESP_OK);
    network_start();
    assert(!reporter_is_ready());
    connect_client(1);
    connect_client(2);
    assign_ip(2, "192.168.4.3"); /* DHCP order must not override association order. */
    usleep(300000);
    assert(atomic_load(&health_requests) == 0);
    assign_ip(1, "192.168.4.2");
    wait_count(&health_requests, 1);
    assert(!reporter_is_ready() && reporter_trigger() == ESP_ERR_INVALID_STATE);
    atomic_store(&health_status, 200); /* The API starts after Wi-Fi connected. */
    wait_ready();
    assert(atomic_load(&health_requests) >= 2);
    assign_ip(2, "192.168.4.3");
    assert(reporter_trigger() == ESP_OK);
    wait_count(&posts_completed, 1);
    expect_post_url("http://192.168.4.2:3000/api/triggers");
    disconnect_client(2);
    assert(reporter_is_ready());

    int checks = atomic_load(&health_requests);
    atomic_store(&post_result, ESP_ERR_TIMEOUT);
    assert(reporter_trigger() == ESP_OK);
    wait_count(&posts_completed, 2);
    wait_count(&health_requests, checks + 1);
    wait_ready();
    usleep(300000);
    assert(atomic_load(&posts_started) == 2); /* The ambiguous POST was not retried. */
    atomic_store(&post_result, ESP_OK);

    connect_client(2);
    atomic_store(&block_post, true);
    assert(reporter_trigger() == ESP_OK);
    wait_count(&posts_started, 3);
    assert(reporter_trigger() == ESP_OK); /* Queue an event from the old host session. */
    disconnect_client(1);
    assert(!reporter_is_ready() && reporter_trigger() == ESP_ERR_INVALID_STATE);
    assign_ip(2, "192.168.4.3"); /* An existing client must reconnect to claim the host. */
    assert(!reporter_is_ready());
    connect_client(3);
    assign_ip(3, "192.168.4.4");
    atomic_store(&block_post, false);
    wait_count(&posts_completed, 3);
    wait_ready();
    usleep(300000);
    assert(atomic_load(&posts_started) == 3); /* Old queued event did not move to new host. */
    assert(reporter_trigger() == ESP_OK);
    wait_count(&posts_completed, 4);
    expect_post_url("http://192.168.4.4:3000/api/triggers");
    disconnect_client(1); /* Stale disconnect must not clear the current host. */
    assert(reporter_is_ready());

    atomic_store(&omit_date, true);
    checks = atomic_load(&health_requests);
    assign_ip(3, "192.168.4.9");
    assert(!reporter_is_ready());
    wait_count(&health_requests, checks + 1);
    assert(!reporter_is_ready());
    atomic_store(&omit_date, false);
    wait_ready();
    assert(reporter_trigger() == ESP_OK);
    wait_count(&posts_completed, 5);
    expect_post_url("http://192.168.4.9:3000/api/triggers");

    atomic_store(&block_post, true);
    assert(reporter_trigger() == ESP_OK);
    wait_count(&posts_started, 6);
    for (int i = 0; i < 8; ++i) assert(reporter_trigger() == ESP_OK);
    assert(reporter_trigger() == ESP_ERR_TIMEOUT);
    disconnect_client(3);
    atomic_store(&block_post, false);
    wait_count(&posts_completed, 6);
    usleep(300000);
    assert(!reporter_is_ready() && atomic_load(&posts_started) == 6);
    assert(reporter_trigger() == ESP_ERR_INVALID_STATE);

    pthread_mutex_lock(&observation_lock);
    puts(last_payload);
    pthread_mutex_unlock(&observation_lock);
    fputs("Host checks passed: first-client selection, DHCP, port, delayed API readiness, "
          "disconnects, host changes, timestamps, bounded queue, and no POST retries.\n", stderr);
    return 0;
}
