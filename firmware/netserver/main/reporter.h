#pragma once

#include <stdbool.h>
#include <time.h>

#include "esp_err.h"
#include "esp_netif_ip_addr.h"

/* Start once before network_start(). Reporting arms automatically when ready. */
esp_err_t reporter_init(void);

/* Non-blocking host selection from network events. NULL clears the host.
 * The base URL is http://<client IPv4>:CONFIG_NETSERVER_SERVER_PORT. */
void reporter_set_host(const esp_ip4_addr_t *address);
bool reporter_is_ready(void);

/* Call on a beam break from a sensor task, never an ISR. Success means queued.
 * Returns ESP_ERR_INVALID_STATE until the selected host's /health succeeds.
 * Triggers are not automatically retried or replayed after a host change. */
esp_err_t reporter_trigger(void);
