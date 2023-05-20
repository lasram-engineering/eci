#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#define APP_STATE_UPDATE BIT0

#define STATE_TYPE_ERROR 0
#define STATE_TYPE_INPUT 1
#define STATE_TYPE_INTERNAL 2

// errors
#define APP_STATE_ERROR_CRITICAL BIT1
#define APP_STATE_ERROR_WIFI_CONNECTION BIT2

// inputs

// internal
#define APP_STATE_INTERNAL_WIFI_CONNECTED BIT1

esp_err_t app_state_init();

void app_state_set(int type, EventBits_t bits);
void app_state_unset(int type, EventBits_t bits);
void app_state_reset(int type);
EventBits_t app_state_get(int type);
void app_state_wait_for_event(int type, EventBits_t event_bits);
