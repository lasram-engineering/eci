#pragma once

#include <stdbool.h>

#include <esp_err.h>
#include <esp_bit_defs.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define WIFI_INITIALIZED BIT0
#define WIFI_CONNECTED BIT1
#define WIFI_ERROR BIT2

#define WIFI_RETURN_IF_NOT_CONNECTED(ret) \
    do                                    \
    {                                     \
        if (!is_wifi_connected())         \
        {                                 \
            return (ret);                 \
        }                                 \
    } while (0)

esp_err_t wifi_connect_to_station();

bool is_wifi_initialized();
bool is_wifi_connected();

esp_err_t wifi_wait_initialized(TickType_t ticks_to_wait);
esp_err_t wifi_wait_connected(TickType_t ticks_to_wait);
