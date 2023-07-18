#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define UART_TASK 1
#define UART_TASK_PRIO MIN(20, configMAX_PRIORITIES)

#define MAU_TASK 1
#define MAU_TASK_PRIO MIN(19, configMAX_PRIORITIES)

void initialize_tasks();