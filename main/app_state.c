#include "app_state.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>

static const char *TAG = "STATE";
static EventGroupHandle_t app_state_error;
static EventGroupHandle_t app_state_input;
static EventGroupHandle_t app_state_internal;

esp_err_t app_state_init()
{
    app_state_error = xEventGroupCreate();
    app_state_input = xEventGroupCreate();
    app_state_internal = xEventGroupCreate();

    if (app_state_error == NULL || app_state_input == NULL || app_state_internal == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void app_state_set(int type, EventBits_t bits)
{
    uint32_t bits_to_set = bits | APP_STATE_UPDATE;
    switch (type)
    {
    case STATE_TYPE_ERROR:
        xEventGroupSetBits(app_state_error, bits_to_set);
        break;

    case STATE_TYPE_INPUT:
        xEventGroupSetBits(app_state_input, bits_to_set);
        break;

    case STATE_TYPE_INTERNAL:
        xEventGroupSetBits(app_state_internal, bits_to_set);
        break;

    default:
        ESP_LOGW(TAG, "Invalid input type for 'app_state_set': %d", type);
        break;
    }
}

void app_state_unset(int type, EventBits_t bits)
{
    uint32_t bits_to_set = bits | APP_STATE_UPDATE;
    switch (type)
    {
    case STATE_TYPE_ERROR:
        xEventGroupClearBits(app_state_error, bits_to_set);
        break;

    case STATE_TYPE_INPUT:
        xEventGroupClearBits(app_state_input, bits_to_set);
        break;

    case STATE_TYPE_INTERNAL:
        xEventGroupClearBits(app_state_internal, bits_to_set);
        break;

    default:
        ESP_LOGW(TAG, "Invalid input type for 'app_state_set': %d", type);
        break;
    }
}

EventBits_t app_state_get(int type)
{
    EventBits_t bits;
    switch (type)
    {
    case STATE_TYPE_ERROR:
        bits = xEventGroupGetBits(app_state_error);
        break;

    case STATE_TYPE_INPUT:
        bits = xEventGroupGetBits(app_state_input);
        break;

    case STATE_TYPE_INTERNAL:
        bits = xEventGroupGetBits(app_state_internal);
        break;

    default:
        ESP_LOGW(TAG, "Invalid input type for 'app_state_get': %d", type);
        bits = 0x0;
        break;
    }
    return bits;
}

void app_state_wait_for_event(int type, EventBits_t event_bits)
{
    switch (type)
    {
    case STATE_TYPE_ERROR:
        xEventGroupWaitBits(app_state_error,
                            event_bits,
                            pdFALSE,
                            pdFALSE,
                            portMAX_DELAY);
        break;

    case STATE_TYPE_INPUT:
        xEventGroupWaitBits(app_state_input,
                            event_bits,
                            pdFALSE,
                            pdFALSE,
                            portMAX_DELAY);
        break;

    case STATE_TYPE_INTERNAL:
        xEventGroupWaitBits(app_state_internal,
                            event_bits,
                            pdFALSE,
                            pdFALSE,
                            portMAX_DELAY);
        break;

    default:
        ESP_LOGW(TAG, "Invalid input type for 'app_state_wait_for_event': %d", type);
        break;
    }
}
