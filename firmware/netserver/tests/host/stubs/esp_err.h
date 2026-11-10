#pragma once
#include <stdlib.h>
typedef int esp_err_t;
enum {
    ESP_OK, ESP_FAIL, ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_ARG,
    ESP_ERR_INVALID_SIZE, ESP_ERR_INVALID_RESPONSE, ESP_ERR_NO_MEM, ESP_ERR_TIMEOUT
};
#define ESP_ERROR_CHECK(expression) do { if ((expression) != ESP_OK) abort(); } while (0)
static inline const char *esp_err_to_name(esp_err_t err) { (void)err; return "test error"; }
