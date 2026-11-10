#pragma once
#include "FreeRTOS.h"
typedef struct fake_queue *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned count, unsigned size);
int xQueueSend(QueueHandle_t queue, const void *value, TickType_t wait);
int xQueueReceive(QueueHandle_t queue, void *value, TickType_t wait);
void vQueueDelete(QueueHandle_t queue);
