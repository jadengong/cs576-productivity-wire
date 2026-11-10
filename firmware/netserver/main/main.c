#include "network.h"
#include "reporter.h"

void app_main(void)
{
    ESP_ERROR_CHECK(reporter_init());
    network_start();
}
