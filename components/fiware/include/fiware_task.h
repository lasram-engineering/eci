#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

extern QueueHandle_t fiware_iota_measurement_queue;

void fiware_task();