#include "app_tasks.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "app_state.h"
#include "io.h"

#ifndef configUSE_16_BIT_TICKS
#define EVENT_GROUP_BITS 24
#else
#define EVENT_GROUP_BITS 8
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static const char *TAG = "STATE_TASKS";

void error_task(void *args)
{
    ESP_LOGI(TAG, "Starting error task");
    while (1)
    {
        // wait for the error state to be updated
        app_state_wait_for_event(STATE_TYPE_ERROR, APP_STATE_ANY);

        EventBits_t error_bits = app_state_get(STATE_TYPE_ERROR);

        int i;
        for (i = 0; i < MIN(EVENT_GROUP_BITS, IO_OUTPUT_NUMBER); i++)
        {
            // check if the bit is set
            if (error_bits & BIT(i))
            {
                // write the error to the output
                set_output_level(i, 1);
            }
            else
            {
                set_output_level(i, 0);
            }
        }

        ESP_LOGI(TAG, "Error event: %lx", error_bits);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void input_task(void *args)
{
    ESP_LOGI(TAG, "Starting input task");
    init_io();
    while (1)
    {
        // wait for the input state to be updated
        app_state_wait_for_event(STATE_TYPE_INPUT, APP_STATE_ANY);

        EventBits_t input_bits = app_state_get(STATE_TYPE_INPUT);

        // reset the input register bits
        app_state_reset(STATE_TYPE_INPUT);

        ESP_LOGI(TAG, "Input event: %lx", input_bits);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void internal_task(void *args)
{
}
