#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_log.h>

esp_err_t process_payload(char *payload, TaskHandle_t mau_task_handle);