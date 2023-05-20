#pragma once

#include "freertos/FreeRTOS.h"

#define APP_STATE_NO_ERROR 0x0
#define APP_STATE_NO_ERROR_MSG "No error"
#define APP_STATE_ERROR_MSG_LEN 20

#define APP_STATE_INTERNAL_ERROR BIT0
#define APP_STATE_WIFI_CONNECT_ERROR BIT1

typedef int32_t app_state_error_flag_t;

typedef struct
{
    unsigned short error;
    char error_msg[APP_STATE_ERROR_MSG_LEN];
    bool wifi_connected;
} app_state_t;

typedef void (*app_state_handler_t)(app_state_t *, void *);

esp_err_t app_state_init();

void app_state_modify(app_state_handler_t handler, void *handler_extra_args);

app_state_t app_state_get();
