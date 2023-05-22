#include "app_state.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>

#define BITS_ALL 0xFFFFFF

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
    BaseType_t xHigherPriorityTaskWoken, xResult;

    /* xHigherPriorityTaskWoken must be initialised to pdFALSE. */
    xHigherPriorityTaskWoken = pdFALSE;

    if (xPortInIsrContext())
    {
        switch (type)
        {
        case STATE_TYPE_ERROR:
            xResult = xEventGroupSetBitsFromISR(app_state_error, bits_to_set, &xHigherPriorityTaskWoken);
            break;

        case STATE_TYPE_INPUT:
            xResult = xEventGroupSetBitsFromISR(app_state_input, bits_to_set, &xHigherPriorityTaskWoken);
            break;

        case STATE_TYPE_INTERNAL:
            xResult = xEventGroupSetBitsFromISR(app_state_internal, bits_to_set, &xHigherPriorityTaskWoken);
            break;

        default:
            xResult = pdFAIL;
            ESP_DRAM_LOGW(TAG, "Invalid input type for 'app_state_set': %d", type);
            break;
        }

        /* Was the message posted successfully? */
        if (xResult != pdFAIL)
        {
            /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
            switch should be requested.  The macro used is port specific and will
            be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
            the documentation page for the port being used. */
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
    else
    {
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
}

void app_state_unset(int type, EventBits_t bits)
{
    uint32_t bits_to_set = bits | APP_STATE_UPDATE;

    if (xPortInIsrContext())
    {
        switch (type)
        {
        case STATE_TYPE_ERROR:
            xEventGroupClearBitsFromISR(app_state_error, bits_to_set);
            break;

        case STATE_TYPE_INPUT:
            xEventGroupClearBitsFromISR(app_state_input, bits_to_set);
            break;

        case STATE_TYPE_INTERNAL:
            xEventGroupClearBitsFromISR(app_state_internal, bits_to_set);
            break;

        default:
            ESP_DRAM_LOGW(TAG, "Invalid input type for 'app_state_set': %d", type);
            break;
        }
    }
    else
    {
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
}

void app_state_reset(int type)
{
    if (xPortInIsrContext())
    {
        switch (type)
        {
        case STATE_TYPE_ERROR:
            xEventGroupClearBitsFromISR(app_state_error, BITS_ALL);
            break;

        case STATE_TYPE_INPUT:
            xEventGroupClearBitsFromISR(app_state_input, BITS_ALL);
            break;

        case STATE_TYPE_INTERNAL:
            xEventGroupClearBitsFromISR(app_state_internal, BITS_ALL);
            break;

        default:
            ESP_DRAM_LOGW(TAG, "Invalid input type for 'app_state_set': %d", type);
            break;
        }
    }
    else
    {
        switch (type)
        {
        case STATE_TYPE_ERROR:
            xEventGroupClearBits(app_state_error, BITS_ALL);
            break;

        case STATE_TYPE_INPUT:
            xEventGroupClearBits(app_state_input, BITS_ALL);
            break;

        case STATE_TYPE_INTERNAL:
            xEventGroupClearBits(app_state_internal, BITS_ALL);
            break;

        default:
            ESP_LOGW(TAG, "Invalid input type for 'app_state_set': %d", type);
            break;
        }
    }
}

EventBits_t app_state_get(int type)
{
    const int in_isr = xPortInIsrContext();

    EventBits_t bits;

    if (in_isr)
    {
        switch (type)
        {
        case STATE_TYPE_ERROR:
            bits = xEventGroupGetBitsFromISR(app_state_error);
            break;

        case STATE_TYPE_INPUT:
            bits = xEventGroupGetBitsFromISR(app_state_input);
            break;

        case STATE_TYPE_INTERNAL:
            bits = xEventGroupGetBitsFromISR(app_state_internal);
            break;

        default:
            ESP_DRAM_LOGW(TAG, "Invalid input type for 'app_state_get': %d", type);
            bits = 0x0;
            break;
        }
    }
    else
    {
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
