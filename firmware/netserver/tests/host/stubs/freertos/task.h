#pragma once
#include "FreeRTOS.h"
int xTaskCreate(void (*task)(void *), const char *name, unsigned stack,
                void *argument, unsigned priority, void *handle);
TickType_t xTaskGetTickCount(void);
